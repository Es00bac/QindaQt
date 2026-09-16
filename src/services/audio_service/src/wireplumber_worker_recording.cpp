// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_worker_p.h"

#include <utility>

namespace QindaQt::Audio
{

void WirePlumberWorker::applyRecording(BackendRecording recording)
{
    invoke([this, recording = std::move(recording)] { applyRecordingOnWorker(recording); });
}

void WirePlumberWorker::applyRecordingOnWorker(const BackendRecording &recording)
{
    m_declaredRecording = recording;
    if (!recording.active) {
        m_recorder.stop();
        return;
    }
    if (m_recorder.running() || m_core == nullptr || m_manager == nullptr) {
        // Already running the declared recording, or nothing to run it on yet;
        // the next rebuild tries again once the graph is there.
        return;
    }
    struct pw_context *const context = wp_core_get_pw_context(m_core);
    const QString node = nodeNameForHandle(recording.target);
    if (context == nullptr || node.isEmpty()) {
        return;
    }
    const bool started = m_recorder.start(
        context, node, recording.path, recording.format, [this](const QString &reason) {
            // From the writer thread: hand it to the worker loop, which owns
            // the recorder and the declaration.
            invoke([this, reason] {
                m_recorder.stop();
                m_declaredRecording = {};
                if (m_recordingFailedCallback) {
                    m_recordingFailedCallback(reason);
                }
            });
        });
    if (!started && m_recordingFailedCallback) {
        m_declaredRecording = {};
        m_recordingFailedCallback(QStringLiteral("recording-start-failed"));
    }
}

} // namespace QindaQt::Audio
