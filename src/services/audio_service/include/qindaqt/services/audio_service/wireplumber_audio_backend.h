// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_backend.h>

#include <memory>

namespace QindaQt::Audio
{

class WirePlumberWorker;

// Production adapter. Every libwireplumber/GObject handle is private to its
// dedicated GLib worker; callers interact only through AudioBackend values.
// start/submit/stop run on the creating Qt thread. stop() synchronously quits
// and joins the worker, making teardown complete before destruction.
class WirePlumberAudioBackend final : public AudioBackend
{
    Q_OBJECT

public:
    explicit WirePlumberAudioBackend(QObject *parent = nullptr);
    ~WirePlumberAudioBackend() override;

    void start() override;
    void stop() override;
    void submit(quint64 operationId, const OperationRequest &request) override;

private:
    std::unique_ptr<WirePlumberWorker> m_worker;
};

} // namespace QindaQt::Audio
