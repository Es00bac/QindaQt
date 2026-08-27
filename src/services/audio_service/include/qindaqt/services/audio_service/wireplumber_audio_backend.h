// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_backend.h>

#include <atomic>
#include <memory>

namespace QindaQt::Audio
{

class WirePlumberWorker;

// Production adapter. Every libwireplumber/GObject handle is private to its
// dedicated GLib worker; callers interact only through AudioBackend values.
// start/submit/stop run on the creating Qt thread. stop() synchronously quits
// and joins the worker; a run-generation fence drops immutable values that were
// queued to Qt before that join completed.
class WirePlumberAudioBackend final : public AudioBackend
{
    Q_OBJECT

public:
    explicit WirePlumberAudioBackend(QObject *parent = nullptr);
    ~WirePlumberAudioBackend() override;

    [[nodiscard]] quint64 start() override;
    void stop() override;
    void submit(quint64 operationId, const OperationRequest &request) override;

private:
    [[nodiscard]] quint64 advanceRunGeneration();

    std::unique_ptr<WirePlumberWorker> m_worker;
    std::atomic<quint64> m_runGeneration = 0;
    std::atomic_bool m_running = false;
};

} // namespace QindaQt::Audio
