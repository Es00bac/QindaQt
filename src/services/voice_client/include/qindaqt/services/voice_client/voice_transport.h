// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_protocol/voice_types.h>

#include <QtCore/QObject>

namespace QindaQt::Services::Voice {

// The seam every VoiceClient consumer injects. Ownership stays with the
// caller; a transport must share the client's thread and outlive it.
//
// AGENT-CONTRACT: a transport reports the owner it answered for with every
// reply. The client discards any reply whose owner is not the one it is
// currently bound to, so a transport must never rewrite that field to the
// current owner on a late reply.
class VoiceTransport : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~VoiceTransport() override = default;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fetchSnapshot(const QString &owner, quint64 token) = 0;
    virtual void submitOperation(const QString &owner, quint64 token,
                                 const OperationRequest &request) = 0;

Q_SIGNALS:
    void ownerChanged(const QString &owner);
    void invalidated(const QString &owner, quint64 revision);
    // Unrevisioned, lossy capture telemetry. Never gates an intent.
    void levelReported(const QString &owner, quint32 levelPercent);
    void snapshotReply(const QString &owner, quint64 token, bool transportSuccess,
                       const QindaQt::Services::Voice::Snapshot &snapshot,
                       const QString &reasonCode);
    void operationReply(const QString &owner, quint64 token, bool transportSuccess,
                        const QindaQt::Services::Voice::OperationResult &result,
                        const QString &reasonCode);
};

} // namespace QindaQt::Services::Voice
