// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wireplumber_recorder_p.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QStandardPaths>

#include <pipewire/pipewire.h>
#include <sndfile.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <mutex>
#include <condition_variable>

namespace QindaQt::Audio
{
namespace {

constexpr uint32_t kRate = 48000;
constexpr uint32_t kChannels = 2;
// Four seconds of stereo float at 48 kHz: enough for a disk hiccup, small
// enough to allocate without thought.
constexpr size_t kRingFrames = kRate * 4;
constexpr char kRecorderNamePrefix[] = "qindaqt.recorder.";

} // namespace

struct Recorder::Impl {
    pw_stream *stream = nullptr;
    spa_hook listener{};
    SNDFILE *file = nullptr;
    std::vector<float> ring;
    // Producer (realtime) writes head; consumer (writer thread) writes tail.
    std::atomic<size_t> head{0};
    std::atomic<size_t> tail{0};
    std::atomic_bool stopping{false};
    std::thread writer;
    std::mutex wake;
    std::condition_variable wakeCondition;
    Recorder *owner = nullptr;
    FailureCallback onFailure;

    void writerLoop()
    {
        std::vector<float> chunk(kRate * kChannels / 4);
        while (true) {
            size_t t = tail.load(std::memory_order_relaxed);
            const size_t h = head.load(std::memory_order_acquire);
            if (t == h) {
                if (stopping.load()) {
                    break;
                }
                std::unique_lock lock(wake);
                wakeCondition.wait_for(lock, std::chrono::milliseconds(50));
                continue;
            }
            size_t count = 0;
            while (t != h && count < chunk.size()) {
                chunk[count++] = ring[t];
                t = (t + 1) % ring.size();
            }
            tail.store(t, std::memory_order_release);
            const sf_count_t frames = static_cast<sf_count_t>(count / kChannels);
            if (frames > 0 && sf_writef_float(file, chunk.data(), frames) != frames) {
                // The disk refused: stop feeding it, tell the owner once.
                stopping.store(true);
                if (onFailure) {
                    onFailure(QStringLiteral("recording-write-failed"));
                }
                break;
            }
        }
    }
};

namespace {

void onProcess(void *userData)
{
    auto *const impl = static_cast<Recorder::Impl *>(userData);
    pw_buffer *const buffer = pw_stream_dequeue_buffer(impl->stream);
    if (buffer == nullptr) {
        return;
    }
    const spa_data &data = buffer->buffer->datas[0];
    const auto *samples = static_cast<const float *>(data.data);
    if (samples != nullptr && data.chunk->size > 0 && !impl->stopping.load()) {
        const size_t count = data.chunk->size / sizeof(float);
        size_t h = impl->head.load(std::memory_order_relaxed);
        const size_t t = impl->tail.load(std::memory_order_acquire);
        const size_t capacity = impl->ring.size();
        const size_t free = (t + capacity - h - 1) % capacity;
        const size_t writable = std::min(count, free);
        for (size_t index = 0; index < writable; ++index) {
            impl->ring[h] = samples[index];
            h = (h + 1) % capacity;
        }
        impl->head.store(h, std::memory_order_release);
        if (writable < count) {
            impl->owner->droppedFramesAdd((count - writable) / kChannels);
        }
        impl->wakeCondition.notify_one();
    }
    pw_stream_queue_buffer(impl->stream, buffer);
}

constexpr pw_stream_events kEvents = {
    .version = PW_VERSION_STREAM_EVENTS,
    .destroy = nullptr,
    .state_changed = nullptr,
    .control_info = nullptr,
    .io_changed = nullptr,
    .param_changed = nullptr,
    .add_buffer = nullptr,
    .remove_buffer = nullptr,
    .process = onProcess,
    .drained = nullptr,
    .command = nullptr,
    .trigger_done = nullptr,
};

} // namespace

Recorder::Recorder()
    : m_impl(std::make_unique<Impl>())
{
    m_impl->owner = this;
}

Recorder::~Recorder()
{
    stop();
}

bool Recorder::start(pw_context *const context, const QString &nodeName, const QString &path,
                     const QString &format, FailureCallback onFailure)
{
    if (m_running.load() || context == nullptr || nodeName.isEmpty() || path.isEmpty()
        || !recordingFormatIsKnown(format)) {
        return false;
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        return false;
    }
    SF_INFO info{};
    info.samplerate = kRate;
    info.channels = kChannels;
    info.format = format == QStringLiteral("flac") ? (SF_FORMAT_FLAC | SF_FORMAT_PCM_24)
                                                   : (SF_FORMAT_WAV | SF_FORMAT_PCM_24);
    m_impl->file = sf_open(path.toUtf8().constData(), SFM_WRITE, &info);
    if (m_impl->file == nullptr) {
        return false;
    }
    m_impl->ring.assign(kRingFrames * kChannels, 0.0F);
    m_impl->head.store(0);
    m_impl->tail.store(0);
    m_impl->stopping.store(false);
    m_impl->onFailure = std::move(onFailure);
    m_dropped.store(0);

    const QByteArray name = (QLatin1String(kRecorderNamePrefix) + nodeName).toUtf8();
    const QByteArray target = nodeName.toUtf8();
    pw_properties *const properties = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE,
        "Production", PW_KEY_NODE_NAME, name.constData(), PW_KEY_TARGET_OBJECT,
        target.constData(), PW_KEY_STREAM_CAPTURE_SINK, "true", PW_KEY_NODE_VIRTUAL, "true",
        // A recording is what the user asked for: it keeps the device running.
        PW_KEY_NODE_PASSIVE, "false", "node.dont-fallback", "true", nullptr);
    m_impl->stream = pw_stream_new_simple(pw_context_get_main_loop(context), name.constData(),
                                          properties, &kEvents, m_impl.get());
    if (m_impl->stream == nullptr) {
        sf_close(m_impl->file);
        m_impl->file = nullptr;
        return false;
    }
    uint8_t podBuffer[1024];
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
    spa_audio_info_raw raw{};
    raw.format = SPA_AUDIO_FORMAT_F32;
    raw.rate = kRate;
    raw.channels = kChannels;
    const spa_pod *params[1] = {spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &raw)};
    if (pw_stream_connect(m_impl->stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                          static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT
                                                       | PW_STREAM_FLAG_MAP_BUFFERS
                                                       | PW_STREAM_FLAG_RT_PROCESS),
                          params, 1)
        < 0) {
        pw_stream_destroy(m_impl->stream);
        m_impl->stream = nullptr;
        sf_close(m_impl->file);
        m_impl->file = nullptr;
        return false;
    }
    m_running.store(true);
    m_impl->writer = std::thread([impl = m_impl.get()] { impl->writerLoop(); });
    return true;
}

void Recorder::stop()
{
    if (!m_running.exchange(false)) {
        return;
    }
    if (m_impl->stream != nullptr) {
        pw_stream_destroy(m_impl->stream);
        m_impl->stream = nullptr;
    }
    m_impl->stopping.store(true);
    m_impl->wakeCondition.notify_all();
    if (m_impl->writer.joinable()) {
        m_impl->writer.join();
    }
    if (m_impl->file != nullptr) {
        sf_close(m_impl->file);
        m_impl->file = nullptr;
    }
    m_impl->ring.clear();
    m_impl->ring.shrink_to_fit();
}

QString recordingPathFor(const QString &busId, const QString &format)
{
    const QByteArray override = qgetenv("QINDAQT_AUDIO_RECORDING_DIR");
    const QString directory = override.isEmpty()
        ? QDir(QStandardPaths::writableLocation(QStandardPaths::MusicLocation))
              .filePath(QStringLiteral("QindaQt Recordings"))
        : QString::fromUtf8(override);
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH-mm-ss"));
    return QDir(directory).filePath(QStringLiteral("%1 %2.%3").arg(busId, stamp, format));
}

bool recordingFormatIsKnown(const QString &format)
{
    return format == QStringLiteral("flac") || format == QStringLiteral("wav");
}

} // namespace QindaQt::Audio
