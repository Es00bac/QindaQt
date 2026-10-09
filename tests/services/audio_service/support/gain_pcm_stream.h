// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <pipewire/pipewire.h>
#include <QtCore/QString>
#include <array>
#include <atomic>
#include <cstdint>

namespace QindaQt::Audio::Test
{
struct PcmWindow {
    uint64_t frames = 0;
    std::array<float, 2> peak{};
};

// Test-only synthetic producer or sink-monitor consumer on an already proven
// private Core. No hardware names, files or host capture are accepted. The RT
// callback uses only bounded buffer access and lock-free atomics.
class GainPcmStream final
{
public:
    enum class Direction { Produce, ObserveSink };
    GainPcmStream() = default;
    ~GainPcmStream();
    GainPcmStream(const GainPcmStream &) = delete;
    GainPcmStream &operator=(const GainPcmStream &) = delete;
    [[nodiscard]] bool start(pw_core *core, const QString &target, Direction direction);
    void resetWindow();
    [[nodiscard]] PcmWindow window() const;
    [[nodiscard]] bool streaming() const;
    void setInputAmplitude(float value);
    static constexpr float kInputAmplitude = 0.001F;

private:
    static void processed(void *);
    static void destroyed(void *);
    static void changed(void *, pw_stream_state, pw_stream_state, const char *);
    pw_stream *m_stream = nullptr;
    spa_hook m_listener{};
    Direction m_direction = Direction::ObserveSink;
    std::atomic<float> m_input{kInputAmplitude};
    std::atomic<uint64_t> m_frames{0};
    std::array<std::atomic<float>, 2> m_peak{};
    std::atomic<pw_stream_state> m_state{PW_STREAM_STATE_UNCONNECTED};
};
} // namespace QindaQt::Audio::Test
