// SPDX-License-Identifier: GPL-3.0-or-later
#include "gain_pcm_stream.h"

#include <spa/param/audio/format-utils.h>
#include <spa/pod/builder.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace QindaQt::Audio::Test
{
static_assert(std::atomic<float>::is_always_lock_free);
static_assert(std::atomic<uint64_t>::is_always_lock_free);

GainPcmStream::~GainPcmStream()
{
    if (m_stream) pw_stream_destroy(m_stream);
}
void GainPcmStream::destroyed(void *data)
{
    auto *self = static_cast<GainPcmStream *>(data);
    self->m_stream = nullptr;
    self->m_state.store(PW_STREAM_STATE_UNCONNECTED, std::memory_order_relaxed);
    spa_hook_remove(&self->m_listener);
}
void GainPcmStream::changed(void *data, pw_stream_state, const pw_stream_state next, const char *)
{
    static_cast<GainPcmStream *>(data)->m_state.store(next, std::memory_order_relaxed);
}
bool GainPcmStream::start(pw_core *core, const QString &target, const Direction direction)
{
    if (!core || m_stream || target.isEmpty()) return false;
    m_direction = direction;
    const QByteArray encoded = target.toUtf8();
    const bool produce = direction == Direction::Produce;
    auto *properties = pw_properties_new(
        PW_KEY_NODE_NAME, produce ? "gain.fixture.generator" : "gain.fixture.observer",
        PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, produce ? "Playback" : "Capture",
        PW_KEY_MEDIA_ROLE, "Production", PW_KEY_TARGET_OBJECT, encoded.constData(),
        "node.dont-fallback", "true", "node.passive", "false",
        "stream.capture.sink", produce ? "false" : "true",
        "audio.position", "[ FL FR ]", "channelmix.normalize", "false",
        "state.restore-props", "false", nullptr);
    m_stream = pw_stream_new(core, produce ? "Private generator" : "Private PCM observer", properties);
    if (!m_stream) return false;
    static const pw_stream_events events = [] {
        pw_stream_events value{}; value.version = PW_VERSION_STREAM_EVENTS;
        value.destroy = destroyed; value.state_changed = changed; value.process = processed;
        return value;
    }();
    pw_stream_add_listener(m_stream, &m_listener, &events, this);
    std::array<uint8_t, 1024> buffer{};
    spa_pod_builder builder = SPA_POD_BUILDER_INIT(buffer.data(), buffer.size());
    spa_audio_info_raw format{};
    format.format = SPA_AUDIO_FORMAT_F32;
    format.rate = 48000; format.channels = 2;
    format.position[0] = SPA_AUDIO_CHANNEL_FL; format.position[1] = SPA_AUDIO_CHANNEL_FR;
    const spa_pod *parameter = spa_format_audio_raw_build(&builder, SPA_PARAM_EnumFormat, &format);
    const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT
        | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS);
    if (pw_stream_connect(m_stream, produce ? PW_DIRECTION_OUTPUT : PW_DIRECTION_INPUT,
                          PW_ID_ANY, flags, &parameter, 1) < 0) {
        pw_stream_destroy(m_stream); return false;
    }
    return true;
}
void GainPcmStream::processed(void *data)
{
    auto *self = static_cast<GainPcmStream *>(data);
    if (!self->m_stream) return;
    pw_buffer *buffer = pw_stream_dequeue_buffer(self->m_stream);
    if (!buffer) return;
    if (buffer->buffer->n_datas != 1) { pw_stream_queue_buffer(self->m_stream, buffer); return; }
    spa_data &block = buffer->buffer->datas[0];
    if (!block.data || !block.chunk) { pw_stream_queue_buffer(self->m_stream, buffer); return; }
    constexpr uint32_t frameBytes = 2 * sizeof(float);
    if (self->m_direction == Direction::Produce) {
        const uint32_t capacity = block.maxsize / frameBytes;
        const uint32_t frames = std::min<uint32_t>(capacity, buffer->requested
            ? static_cast<uint32_t>(std::min<uint64_t>(buffer->requested,
                                  std::numeric_limits<uint32_t>::max())) : 1024);
        auto *samples = static_cast<float *>(block.data);
        const float value = self->m_input.load(std::memory_order_relaxed);
        std::fill_n(samples, static_cast<size_t>(frames) * 2, value);
        block.chunk->offset = 0; block.chunk->size = frames * frameBytes;
        block.chunk->stride = static_cast<int32_t>(frameBytes);
        self->m_frames.fetch_add(frames, std::memory_order_relaxed);
    } else if (block.chunk->offset <= block.maxsize
               && block.chunk->stride == static_cast<int32_t>(frameBytes)) {
        const uint32_t bytes = std::min(block.chunk->size, block.maxsize - block.chunk->offset);
        const uint32_t frames = bytes / frameBytes;
        const auto *samples = reinterpret_cast<const float *>(
            static_cast<const uint8_t *>(block.data) + block.chunk->offset);
        std::array<float, 2> peak{};
        for (uint32_t frame = 0; frame < frames; ++frame) {
            for (size_t channel = 0; channel < 2; ++channel) {
                const float sample = samples[static_cast<size_t>(frame) * 2 + channel];
                if (std::isfinite(sample)) peak[channel] = std::max(peak[channel], std::fabs(sample));
                else peak[channel] = std::numeric_limits<float>::infinity();
            }
        }
        for (size_t channel = 0; channel < 2; ++channel) {
            float previous = self->m_peak[channel].load(std::memory_order_relaxed);
            while (peak[channel] > previous && !self->m_peak[channel].compare_exchange_weak(
                previous, peak[channel], std::memory_order_relaxed)) {}
        }
        self->m_frames.fetch_add(frames, std::memory_order_relaxed);
    }
    pw_stream_queue_buffer(self->m_stream, buffer);
}
void GainPcmStream::resetWindow()
{
    m_frames.store(0, std::memory_order_relaxed);
    for (auto &peak : m_peak) peak.store(0.0F, std::memory_order_relaxed);
}
PcmWindow GainPcmStream::window() const
{
    PcmWindow value;
    value.frames = m_frames.load(std::memory_order_relaxed);
    for (size_t channel = 0; channel < 2; ++channel)
        value.peak[channel] = m_peak[channel].load(std::memory_order_relaxed);
    return value;
}
bool GainPcmStream::streaming() const
{
    return m_state.load(std::memory_order_relaxed) == PW_STREAM_STATE_STREAMING;
}
void GainPcmStream::setInputAmplitude(const float value)
{
    if (std::isfinite(value) && value >= 0.0F && value <= kInputAmplitude)
        m_input.store(value, std::memory_order_relaxed);
}
} // namespace QindaQt::Audio::Test
