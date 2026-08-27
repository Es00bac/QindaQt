// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/audio_service/wireplumber_audio_backend.h>

#include "wireplumber_worker_p.h"

#include <QtCore/QMetaObject>
#include <QtCore/QRandomGenerator>

#include <limits>

namespace QindaQt::Audio
{

WirePlumberAudioBackend::WirePlumberAudioBackend(QObject *parent)
    : AudioBackend(parent)
{
    // Reserve half the counter space for authority changes within this process;
    // a random all-ones epoch would otherwise be unable to advance on restart.
    quint64 initialEpoch = QRandomGenerator::global()->generate64()
        & (std::numeric_limits<quint64>::max() / 2);
    if (initialEpoch == 0) {
        initialEpoch = 1;
    }
    m_worker = std::make_unique<WirePlumberWorker>(
        initialEpoch,
        [this](Snapshot snapshot) {
            const quint64 generation = m_runGeneration.load(std::memory_order_acquire);
            if (!m_running.load(std::memory_order_acquire)) {
                return;
            }
            QMetaObject::invokeMethod(
                this,
                [this, generation, snapshot = std::move(snapshot)] {
                    if (!m_running.load(std::memory_order_acquire)
                        || generation
                            != m_runGeneration.load(std::memory_order_acquire)) {
                        return;
                    }
                    Q_EMIT snapshotReady(generation, snapshot);
                },
                Qt::QueuedConnection);
        },
        [this](const quint64 operationId, BackendOperationOutcome outcome) {
            const quint64 generation = m_runGeneration.load(std::memory_order_acquire);
            if (!m_running.load(std::memory_order_acquire)) {
                return;
            }
            QMetaObject::invokeMethod(
                this,
                [this, generation, operationId, outcome = std::move(outcome)] {
                    if (!m_running.load(std::memory_order_acquire)
                        || generation
                            != m_runGeneration.load(std::memory_order_acquire)) {
                        return;
                    }
                    Q_EMIT operationFinished(generation, operationId, outcome);
                },
                Qt::QueuedConnection);
        });
}

WirePlumberAudioBackend::~WirePlumberAudioBackend()
{
    stop();
}

quint64 WirePlumberAudioBackend::advanceRunGeneration()
{
    const quint64 current = m_runGeneration.load(std::memory_order_relaxed);
    const quint64 next = current == std::numeric_limits<quint64>::max()
        ? 1
        : current + 1;
    m_runGeneration.store(next, std::memory_order_release);
    return next;
}

quint64 WirePlumberAudioBackend::start()
{
    if (m_running.load(std::memory_order_acquire)) {
        return m_runGeneration.load(std::memory_order_acquire);
    }
    const quint64 generation = advanceRunGeneration();
    m_running.store(true, std::memory_order_release);
    m_worker->start();
    return generation;
}

void WirePlumberAudioBackend::stop()
{
    if (!m_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    (void)advanceRunGeneration();
    m_worker->stop();
}

void WirePlumberAudioBackend::submit(const quint64 operationId,
                                     const OperationRequest &request)
{
    if (!m_running.load(std::memory_order_acquire)) {
        return;
    }
    m_worker->submit(operationId, request);
}

} // namespace QindaQt::Audio
