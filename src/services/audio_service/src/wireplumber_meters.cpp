// SPDX-License-Identifier: LGPL-3.0-or-later

#include "wireplumber_meters_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace QindaQt::Audio
{
namespace {

// Metering is a diagnostic read, not a mix path: one fixed format keeps the
// RT callback to a single tight loop, and PipeWire's adapter converts whatever
// the node produces into it.
constexpr uint32_t kMeterRate = 48000;
constexpr uint32_t kMeterChannels = 2;
constexpr int kMeterQuantum = 1024;
// Deliberately NOT the virtual-device prefix: code that enumerates virtual
// devices by name must not find meters, which are internal and unroutable.
constexpr char kMeterNamePrefix[] = "qindaqt.meter.";

[[nodiscard]] double dbFromAmplitude(const double amplitude)
{
    if (!(amplitude > 0.0)) {
        return kSilentMeterDb;
    }
    return std::clamp(20.0 * std::log10(amplitude), kSilentMeterDb, kMaxMeterDb);
}

} // namespace

struct MeterBank::Stream final {
    pw_stream *stream = nullptr;
    QString nodeName;
    bool captureSink = false;
    spa_hook listener{};
    // AGENT-GUARD: the only state shared with PipeWire's realtime data thread.
    // Written exclusively by on_process, read and reset exclusively by the
    // worker loop. Anything more than these - a lock, an allocation, a Qt
    // object - would be a realtime violation and would show up as audible
    // dropouts rather than as a crash.
    std::atomic<float> peakHold{0.0F};
    std::atomic<float> sumSquares{0.0F};
    std::atomic<uint32_t> sampleCount{0};

    ~Stream()
    {
        if (stream != nullptr) {
            pw_stream_destroy(stream);
        }
    }
};

namespace {

void onProcess(void *userData)
{
    auto *const self = static_cast<MeterBank::Stream *>(userData);
    pw_buffer *const buffer = pw_stream_dequeue_buffer(self->stream);
    if (buffer == nullptr) {
        return;
    }
    const spa_data &data = buffer->buffer->datas[0];
    const auto *samples = static_cast<const float *>(data.data);
    if (samples != nullptr && data.chunk->size > 0 && data.chunk->stride > 0) {
        const uint32_t count = data.chunk->size / sizeof(float);
        float peak = 0.0F;
        float energy = 0.0F;
        for (uint32_t index = 0; index < count; ++index) {
            const float value = std::fabs(samples[index]);
            peak = std::max(peak, value);
            energy += samples[index] * samples[index];
        }
        // Peak is a HOLD until the consumer reads it: a transient between two
        // polls must still light the meter, which is the whole point of a peak
        // meter as opposed to a sampled level.
        float previousPeak = self->peakHold.load(std::memory_order_relaxed);
        while (peak > previousPeak
               && !self->peakHold.compare_exchange_weak(previousPeak, peak,
                                                        std::memory_order_relaxed)) {
        }
        self->sumSquares.fetch_add(energy, std::memory_order_relaxed);
        self->sampleCount.fetch_add(count, std::memory_order_relaxed);
    }
    pw_stream_queue_buffer(self->stream, buffer);
}

// Every member is named: the project builds with -Werror=missing-field-
// initializers precisely so a future PipeWire adding a callback cannot leave
// one silently uninitialised in a struct we hand to C.
constexpr pw_stream_events kMeterEvents = {
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

MeterBank::MeterBank() = default;
MeterBank::~MeterBank() = default;

void MeterBank::configure(pw_context *const context, const QList<Target> &targets)
{
    if (context == nullptr) {
        clear();
        return;
    }
    QHash<QString, const Target *> wanted;
    for (const Target &target : targets) {
        if (!target.consoleId.isEmpty() && !target.nodeName.isEmpty()) {
            wanted.insert(target.consoleId, &target);
        }
    }
    for (auto it = m_streams.begin(); it != m_streams.end();) {
        const auto want = wanted.constFind(it->first);
        // Same id, different node or direction - a strip whose rack just
        // started - is a different meter: the old stream would keep reading
        // the device and the meter would never show the processed signal.
        const bool keep = want != wanted.cend() && it->second->nodeName == (*want)->nodeName
            && it->second->captureSink == (*want)->captureSink;
        it = keep ? std::next(it) : m_streams.erase(it);
    }

    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        if (m_streams.count(it.key()) != 0) {
            continue;
        }
        auto stream = std::make_unique<Stream>();
        stream->nodeName = it.value()->nodeName;
        stream->captureSink = it.value()->captureSink;
        const QByteArray meterName =
            (QString::fromLatin1(kMeterNamePrefix) + it.key()).toUtf8();
        const QByteArray targetName = it.value()->nodeName.toUtf8();
        pw_properties *const properties = pw_properties_new(
            PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
            PW_KEY_MEDIA_ROLE, "Production", PW_KEY_NODE_NAME,
            meterName.constData(),
            // target.object, not the deprecated node.target: only this one
            // accepts a node NAME. node.target wants an object id and silently
            // falls back to autoconnecting the default device, which would have
            // every bus meter reading the default output.
            PW_KEY_TARGET_OBJECT, targetName.constData(),
            // A meter must never appear as somewhere the user can route audio.
            PW_KEY_NODE_VIRTUAL, "true", PW_KEY_NODE_PASSIVE,
            it.value()->passive ? "true" : "false",
            PW_KEY_STREAM_CAPTURE_SINK,
            it.value()->captureSink ? "true" : "false", nullptr);
        pw_properties_setf(properties, PW_KEY_NODE_LATENCY, "%d/%u", kMeterQuantum,
                           kMeterRate);

        stream->stream = pw_stream_new_simple(pw_context_get_main_loop(context),
                                              meterName.constData(), properties,
                                              &kMeterEvents, stream.get());
        if (stream->stream == nullptr) {
            continue;
        }
        uint8_t podBuffer[1024];
        spa_pod_builder builder = SPA_POD_BUILDER_INIT(podBuffer, sizeof(podBuffer));
        spa_audio_info_raw info{};
        info.format = SPA_AUDIO_FORMAT_F32;
        info.rate = kMeterRate;
        info.channels = kMeterChannels;
        const spa_pod *params[1] = {
            spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
        if (pw_stream_connect(stream->stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                              static_cast<pw_stream_flags>(
                                  PW_STREAM_FLAG_AUTOCONNECT
                                  | PW_STREAM_FLAG_MAP_BUFFERS
                                  | PW_STREAM_FLAG_RT_PROCESS),
                              params, 1)
            < 0) {
            continue;
        }
        m_streams.emplace(it.key(), std::move(stream));
    }
}

QHash<QString, Level> MeterBank::takeReadings()
{
    QHash<QString, Level> readings;
    for (auto it = m_streams.cbegin(); it != m_streams.cend(); ++it) {
        Stream *const stream = it->second.get();
        const float peak = stream->peakHold.exchange(0.0F, std::memory_order_relaxed);
        const float energy =
            stream->sumSquares.exchange(0.0F, std::memory_order_relaxed);
        const uint32_t count =
            stream->sampleCount.exchange(0, std::memory_order_relaxed);
        Level level;
        if (count == 0) {
            // No audio has been delivered since the last read. The meter is
            // reported as UNKNOWN rather than as silence, so a console draws an
            // idle meter instead of implying the source is running and quiet.
            level.known = false;
            readings.insert(it->first, level);
            continue;
        }
        level.known = true;
        level.peakDb = dbFromAmplitude(static_cast<double>(peak));
        level.rmsDb = dbFromAmplitude(
            std::sqrt(static_cast<double>(energy) / static_cast<double>(count)));
        // RMS can never exceed peak; clamping here keeps the admission gate's
        // invariant true even against floating-point drift.
        level.rmsDb = std::min(level.rmsDb, level.peakDb);
        readings.insert(it->first, level);
    }
    return readings;
}

void MeterBank::clear()
{
    m_streams.clear();
}

} // namespace QindaQt::Audio
