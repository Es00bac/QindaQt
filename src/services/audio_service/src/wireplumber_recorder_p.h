// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <vector>

struct pw_context;

namespace QindaQt::Audio
{

// The recorder (ADR-0184): one pw_stream capturing a bus's device from its
// monitor, and one writer thread turning the samples into a FLAC or WAV file
// through libsndfile.
//
// AGENT-CONTRACT: the realtime callback never blocks and never touches the
// file: it copies samples into a lock-free ring and returns. The writer thread
// drains the ring at its own pace. If the ring ever fills - a stalled disk -
// the realtime side drops audio and counts it; the file stays valid and the
// count is reported when the recording stops.
class Recorder final
{
public:
    using FailureCallback = std::function<void(QString)>;

    Recorder();
    ~Recorder();
    Recorder(const Recorder &) = delete;
    Recorder &operator=(const Recorder &) = delete;

    // Starts capturing `nodeName` into `path`. Returns false when the file
    // cannot be opened or the stream cannot be created; nothing is running
    // then. `format` is "flac" or "wav".
    [[nodiscard]] bool start(pw_context *context, const QString &nodeName,
                             const QString &path, const QString &format,
                             FailureCallback onFailure);
    // Stops, drains what the ring still holds, closes the file.
    void stop();
    [[nodiscard]] bool running() const noexcept { return m_running.load(); }
    [[nodiscard]] quint64 droppedFrames() const noexcept { return m_dropped.load(); }

    struct Impl;
    // Called from the realtime callback only.
    void droppedFramesAdd(const quint64 frames) noexcept
    {
        m_dropped.fetch_add(frames, std::memory_order_relaxed);
    }

private:
    std::unique_ptr<Impl> m_impl;
    std::atomic_bool m_running{false};
    std::atomic<quint64> m_dropped{0};
};

// The file a recording of `busId` goes to, under the recordings directory
// (QINDAQT_AUDIO_RECORDING_DIR, else ~/Music/QindaQt Recordings), named by the
// bus and the moment it started.
[[nodiscard]] QString recordingPathFor(const QString &busId, const QString &format);
[[nodiscard]] bool recordingFormatIsKnown(const QString &format);

} // namespace QindaQt::Audio
