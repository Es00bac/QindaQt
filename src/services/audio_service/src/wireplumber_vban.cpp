// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wireplumber_vban_p.h"

#include "vban_packet_p.h"

#include <QtCore/QByteArray>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include <vector>

namespace QindaQt::Audio
{
namespace {

constexpr uint32_t kRate = 48000;
constexpr uint32_t kChannels = 2;
constexpr uint32_t kFramesPerPacket = 256;
constexpr size_t kRingFrames = kRate;   // one second each way
constexpr char kSenderPrefix[] = "qindaqt.vban.send.";
constexpr char kReceiverPrefix[] = "qindaqt.vban.";

struct Ring {
    std::vector<float> data;
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};

    void reset(const size_t samples)
    {
        data.assign(samples, 0.0F);
        head.store(0);
        tail.store(0);
    }
    // Producer side; returns samples written.
    size_t push(const float *samples, const size_t count)
    {
        size_t h = head.load(std::memory_order_relaxed);
        const size_t t = tail.load(std::memory_order_acquire);
        const size_t capacity = data.size();
        const size_t free = (t + capacity - h - 1) % capacity;
        const size_t n = std::min(count, free);
        for (size_t i = 0; i < n; ++i) {
            data[h] = samples[i];
            h = (h + 1) % capacity;
        }
        head.store(h, std::memory_order_release);
        return n;
    }
    // Consumer side; returns samples read, the rest of `out` untouched.
    size_t pop(float *out, const size_t count)
    {
        size_t t = tail.load(std::memory_order_relaxed);
        const size_t h = head.load(std::memory_order_acquire);
        size_t n = 0;
        while (t != h && n < count) {
            out[n++] = data[t];
            t = (t + 1) % data.size();
        }
        tail.store(t, std::memory_order_release);
        return n;
    }
    [[nodiscard]] size_t available() const
    {
        const size_t h = head.load(std::memory_order_acquire);
        const size_t t = tail.load(std::memory_order_acquire);
        return (h + data.size() - t) % data.size();
    }
};

constexpr pw_stream_events kStreamEvents(void (*process)(void *))
{
    return pw_stream_events{.version = PW_VERSION_STREAM_EVENTS,
                            .destroy = nullptr,
                            .state_changed = nullptr,
                            .control_info = nullptr,
                            .io_changed = nullptr,
                            .param_changed = nullptr,
                            .add_buffer = nullptr,
                            .remove_buffer = nullptr,
                            .process = process,
                            .drained = nullptr,
                            .command = nullptr,
                            .trigger_done = nullptr};
}

pw_stream *connectStream(pw_context *context, const QByteArray &name, pw_properties *properties,
                         const pw_stream_events *events, void *data, const pw_direction direction)
{
    pw_stream *const stream = pw_stream_new_simple(pw_context_get_main_loop(context),
                                                   name.constData(), properties, events, data);
    if (stream == nullptr) {
        return nullptr;
    }
    uint8_t podBuffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
    spa_audio_info_raw raw{};
    raw.format = SPA_AUDIO_FORMAT_F32;
    raw.rate = kRate;
    raw.channels = kChannels;
    const spa_pod *params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &raw)};
    if (pw_stream_connect(stream, direction, PW_ID_ANY,
                          static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT
                                                       | PW_STREAM_FLAG_MAP_BUFFERS
                                                       | PW_STREAM_FLAG_RT_PROCESS),
                          params, 1)
        < 0) {
        pw_stream_destroy(stream);
        return nullptr;
    }
    return stream;
}

int openUdpSocket()
{
    return ::socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
}

} // namespace

// ---------------------------------------------------------------- sender ----

struct VbanSender::Impl {
    pw_stream *stream = nullptr;
    Ring ring;
    int socket = -1;
    sockaddr_in destination{};
    QByteArray streamName;
    std::atomic_bool stopping{false};
    std::thread worker;
    uint32_t frameCounter = 0;

    void loop()
    {
        std::vector<float> frames(kFramesPerPacket * kChannels);
        std::vector<int16_t> pcm(kFramesPerPacket * kChannels);
        Vban::Header header;
        header.samplesPerFrame = kFramesPerPacket;
        header.channels = kChannels;
        header.dataType = Vban::kFormatPcm16;
        header.streamName = QString::fromLatin1(streamName);
        while (!stopping.load()) {
            if (ring.available() < frames.size()) {
                // A packet's worth is 5.3 ms; sleep a little under that.
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
                continue;
            }
            ring.pop(frames.data(), frames.size());
            for (size_t i = 0; i < frames.size(); ++i) {
                pcm[i] = static_cast<int16_t>(std::lround(std::clamp(frames[i], -1.0F, 1.0F) * 32767.0F));
            }
            header.frameCounter = frameCounter++;
            QByteArray packet = Vban::encodeHeader(header);
            packet.append(reinterpret_cast<const char *>(pcm.data()),
                          static_cast<qsizetype>(pcm.size() * sizeof(int16_t)));
            (void)::sendto(socket, packet.constData(), static_cast<size_t>(packet.size()), 0,
                           reinterpret_cast<const sockaddr *>(&destination), sizeof(destination));
        }
    }
};

namespace {

void onSenderProcess(void *data)
{
    auto *const impl = static_cast<VbanSender::Impl *>(data);
    pw_buffer *const buffer = pw_stream_dequeue_buffer(impl->stream);
    if (buffer == nullptr) {
        return;
    }
    const spa_data &chunk = buffer->buffer->datas[0];
    const auto *samples = static_cast<const float *>(chunk.data);
    if (samples != nullptr && chunk.chunk->size > 0) {
        impl->ring.push(samples, chunk.chunk->size / sizeof(float));
    }
    pw_stream_queue_buffer(impl->stream, buffer);
}

constexpr pw_stream_events kSenderEvents = kStreamEvents(onSenderProcess);

} // namespace

VbanSender::VbanSender()
    : m_impl(std::make_unique<Impl>())
{
}

VbanSender::~VbanSender()
{
    stop();
}

bool VbanSender::start(pw_context *const context, const QString &nodeName,
                       const QString &streamName, const QString &host, const quint16 port)
{
    if (m_running.load() || context == nullptr || nodeName.isEmpty() || streamName.isEmpty()) {
        return false;
    }
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo *resolved = nullptr;
    if (::getaddrinfo(host.toUtf8().constData(), nullptr, &hints, &resolved) != 0
        || resolved == nullptr) {
        return false;
    }
    std::memcpy(&m_impl->destination, resolved->ai_addr, sizeof(sockaddr_in));
    ::freeaddrinfo(resolved);
    m_impl->destination.sin_port = htons(port);
    m_impl->socket = openUdpSocket();
    if (m_impl->socket < 0) {
        return false;
    }
    m_impl->streamName = streamName.toLatin1().left(Vban::kStreamNameBytes);
    m_impl->ring.reset(kRingFrames * kChannels);
    m_impl->stopping.store(false);
    m_impl->frameCounter = 0;

    const QByteArray name = (QLatin1String(kSenderPrefix) + streamName).toUtf8();
    const QByteArray target = nodeName.toUtf8();
    pw_properties *const properties = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE,
        "Production", PW_KEY_NODE_NAME, name.constData(), PW_KEY_TARGET_OBJECT, target.constData(),
        PW_KEY_STREAM_CAPTURE_SINK, "true", PW_KEY_NODE_VIRTUAL, "true", PW_KEY_NODE_PASSIVE,
        "false", "node.dont-fallback", "true", nullptr);
    m_impl->stream = connectStream(context, name, properties, &kSenderEvents, m_impl.get(),
                                   PW_DIRECTION_INPUT);
    if (m_impl->stream == nullptr) {
        ::close(m_impl->socket);
        m_impl->socket = -1;
        return false;
    }
    m_running.store(true);
    m_impl->worker = std::thread([impl = m_impl.get()] { impl->loop(); });
    return true;
}

void VbanSender::stop()
{
    if (!m_running.exchange(false)) {
        return;
    }
    if (m_impl->stream != nullptr) {
        pw_stream_destroy(m_impl->stream);
        m_impl->stream = nullptr;
    }
    m_impl->stopping.store(true);
    if (m_impl->worker.joinable()) {
        m_impl->worker.join();
    }
    if (m_impl->socket >= 0) {
        ::close(m_impl->socket);
        m_impl->socket = -1;
    }
}

// -------------------------------------------------------------- receiver ----

struct VbanReceiver::Impl {
    pw_stream *stream = nullptr;
    Ring ring;
    int socket = -1;
    QByteArray streamName;
    std::atomic_bool stopping{false};
    std::thread worker;
    // Silence until a quarter of a packet's worth of margin has arrived, so a
    // burst of late packets does not turn into a stutter.
    std::atomic_bool primed{false};

    void loop()
    {
        std::vector<char> datagram(Vban::kMaxPacketBytes);
        std::vector<float> frames(Vban::kMaxSamplesPerFrame * Vban::kMaxChannels);
        while (!stopping.load()) {
            pollfd descriptor{.fd = socket, .events = POLLIN, .revents = 0};
            if (::poll(&descriptor, 1, 50) <= 0) {
                continue;
            }
            const ssize_t received = ::recv(socket, datagram.data(), datagram.size(), 0);
            if (received <= 0) {
                continue;
            }
            const QByteArray packet = QByteArray::fromRawData(datagram.data(), static_cast<qsizetype>(received));
            const auto header = Vban::decodeHeader(packet);
            if (!header.has_value() || header->streamName.toLatin1() != streamName) {
                continue;
            }
            // Fold whatever channel count arrives into stereo.
            const int in = header->channels;
            const int bytes = Vban::bytesPerSample(header->dataType);
            const char *payload = packet.constData() + Vban::kHeaderBytes;
            size_t out = 0;
            for (int frame = 0; frame < header->samplesPerFrame; ++frame) {
                float left = 0.0F;
                float right = 0.0F;
                for (int channel = 0; channel < in; ++channel) {
                    float value = 0.0F;
                    const char *sample = payload + (frame * in + channel) * bytes;
                    if (header->dataType == Vban::kFormatPcm16) {
                        int16_t pcm = 0;
                        std::memcpy(&pcm, sample, sizeof(pcm));
                        value = static_cast<float>(pcm) / 32768.0F;
                    } else {
                        std::memcpy(&value, sample, sizeof(value));
                    }
                    if (in == 1 || channel % 2 == 0) {
                        left += value;
                    }
                    if (in == 1 || channel % 2 == 1) {
                        right += value;
                    }
                }
                frames[out++] = left;
                frames[out++] = right;
            }
            ring.push(frames.data(), out);
            if (ring.available() >= kFramesPerPacket * kChannels / 4) {
                primed.store(true);
            }
        }
    }
};

namespace {

void onReceiverProcess(void *data)
{
    auto *const impl = static_cast<VbanReceiver::Impl *>(data);
    pw_buffer *const buffer = pw_stream_dequeue_buffer(impl->stream);
    if (buffer == nullptr) {
        return;
    }
    spa_data &chunk = buffer->buffer->datas[0];
    auto *out = static_cast<float *>(chunk.data);
    const uint32_t stride = sizeof(float) * kChannels;
    uint32_t frames = chunk.maxsize / stride;
    if (buffer->requested > 0) {
        frames = std::min<uint32_t>(frames, static_cast<uint32_t>(buffer->requested));
    }
    const size_t wanted = static_cast<size_t>(frames) * kChannels;
    size_t got = 0;
    if (out != nullptr && impl->primed.load()) {
        got = impl->ring.pop(out, wanted);
        if (got < wanted) {
            impl->primed.store(false);   // ran dry: silence until refilled
        }
    }
    if (out != nullptr && got < wanted) {
        std::memset(out + got, 0, (wanted - got) * sizeof(float));
    }
    chunk.chunk->offset = 0;
    chunk.chunk->stride = static_cast<int32_t>(stride);
    chunk.chunk->size = frames * stride;
    pw_stream_queue_buffer(impl->stream, buffer);
}

constexpr pw_stream_events kReceiverEvents = kStreamEvents(onReceiverProcess);

} // namespace

VbanReceiver::VbanReceiver()
    : m_impl(std::make_unique<Impl>())
{
}

VbanReceiver::~VbanReceiver()
{
    stop();
}

bool VbanReceiver::start(pw_context *const context, const QString &streamName, const quint16 port)
{
    if (m_running.load() || context == nullptr || streamName.isEmpty()) {
        return false;
    }
    m_impl->socket = openUdpSocket();
    if (m_impl->socket < 0) {
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    const int reuse = 1;
    ::setsockopt(m_impl->socket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (::bind(m_impl->socket, reinterpret_cast<const sockaddr *>(&address), sizeof(address)) < 0) {
        ::close(m_impl->socket);
        m_impl->socket = -1;
        return false;
    }
    m_impl->streamName = streamName.toLatin1().left(Vban::kStreamNameBytes);
    m_impl->ring.reset(kRingFrames * kChannels);
    m_impl->stopping.store(false);
    m_impl->primed.store(false);

    const QByteArray name = vbanSourceNodeName(streamName).toUtf8();
    const QByteArray description = (QStringLiteral("VBAN ") + streamName).toUtf8();
    pw_properties *const properties = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Playback", PW_KEY_MEDIA_ROLE,
        "Production", PW_KEY_NODE_NAME, name.constData(), PW_KEY_NODE_DESCRIPTION,
        description.constData(),
        // A virtual SOURCE: other applications and the console's strips read
        // it like a microphone.
        PW_KEY_MEDIA_CLASS, "Audio/Source", PW_KEY_NODE_VIRTUAL, "true", PW_KEY_NODE_AUTOCONNECT,
        "false", nullptr);
    m_impl->stream = connectStream(context, name, properties, &kReceiverEvents, m_impl.get(),
                                   PW_DIRECTION_OUTPUT);
    if (m_impl->stream == nullptr) {
        ::close(m_impl->socket);
        m_impl->socket = -1;
        return false;
    }
    m_running.store(true);
    m_impl->worker = std::thread([impl = m_impl.get()] { impl->loop(); });
    return true;
}

void VbanReceiver::stop()
{
    if (!m_running.exchange(false)) {
        return;
    }
    if (m_impl->stream != nullptr) {
        pw_stream_destroy(m_impl->stream);
        m_impl->stream = nullptr;
    }
    m_impl->stopping.store(true);
    if (m_impl->worker.joinable()) {
        m_impl->worker.join();
    }
    if (m_impl->socket >= 0) {
        ::close(m_impl->socket);
        m_impl->socket = -1;
    }
}

QString vbanSourceNodeName(const QString &streamName)
{
    return QLatin1String(kReceiverPrefix) + streamName;
}

} // namespace QindaQt::Audio
