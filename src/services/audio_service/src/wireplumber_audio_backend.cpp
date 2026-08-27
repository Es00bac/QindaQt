// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/audio_service/wireplumber_audio_backend.h>

#include "wireplumber_worker_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QRandomGenerator>

namespace QindaQt::Audio
{

WirePlumberAudioBackend::WirePlumberAudioBackend(QObject *parent)
    : AudioBackend(parent)
{
    quint64 initialEpoch = QRandomGenerator::global()->generate64();
    if (initialEpoch == 0) {
        initialEpoch = 1;
    }
    m_worker = std::make_unique<WirePlumberWorker>(
        initialEpoch,
        [this](Snapshot snapshot) {
            QMetaObject::invokeMethod(
                this,
                [this, snapshot = std::move(snapshot)] {
                    Q_EMIT snapshotReady(snapshot);
                },
                Qt::QueuedConnection);
        },
        [this](const quint64 operationId, BackendOperationOutcome outcome) {
            QMetaObject::invokeMethod(
                this,
                [this, operationId, outcome = std::move(outcome)] {
                    Q_EMIT operationFinished(operationId, outcome);
                },
                Qt::QueuedConnection);
        });
}

WirePlumberAudioBackend::~WirePlumberAudioBackend()
{
    stop();
}

void WirePlumberAudioBackend::start()
{
    m_worker->start();
}

void WirePlumberAudioBackend::stop()
{
    m_worker->stop();
}

void WirePlumberAudioBackend::submit(const quint64 operationId,
                                     const OperationRequest &request)
{
    m_worker->submit(operationId, request);
}

} // namespace QindaQt::Audio
