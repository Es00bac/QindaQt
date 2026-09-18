// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "audio_applet_model.h"

#include <qindaqt/services/audio_client/audio_client.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QVariantList>

namespace QindaQt::Shell::AudioApplet {

// Shell-side facade over the public Audio1 client for the panel applet. This
// is the only object QML sees: it projects bounded rows and phase state and
// accepts volume/mute requests, and it never exposes the client, snapshots,
// handles, or D-Bus objects to QML. Shell composition injects the instance;
// QML neither creates nor looks it up.
//
// AGENT-CONTRACT: The borrowed AudioClient must outlive this controller and
// share its thread. The controller never starts or stops the client; shell
// composition owns that lifecycle. Pending bookkeeping is keyed by the
// protocol's snapshot-unique serial so devices and streams share one
// identity space, exactly as Audio1 guarantees.
//
// AGENT-CONTRACT: The manifest/policy grants are evaluated once by shell
// composition and injected here. Read denial suppresses observation
// entirely; control is effective only together with read, and control
// denial keeps rows visible but rejects every mutation before dispatch.
//
// AGENT-NOTE: A pending entry whose serial disappears from the current
// snapshot is dropped from both maps without feedback; any late result for
// that request then arrives as an unknown request ID and is ignored. This is
// the deliberate bounded stale-cleanup path, not an operation replay.
class AudioAppletController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateReprojected)
    Q_PROPERTY(bool controlGranted READ isControlGranted NOTIFY
                   stateReprojected)
    Q_PROPERTY(QString phaseReasonText READ phaseReasonText NOTIFY
                   stateReprojected)
    Q_PROPERTY(QVariantList deviceRows READ deviceRows NOTIFY stateReprojected)
    Q_PROPERTY(QVariantList streamRows READ streamRows NOTIFY stateReprojected)
    // The console's strips (ADR-0181), and their meters on a narrow channel
    // of their own so a level frame never reprojects the applet.
    Q_PROPERTY(QVariantList consoleRows READ consoleRows NOTIFY stateReprojected)
    Q_PROPERTY(QVariantMap consoleLevels READ consoleLevels NOTIFY consoleLevelsChanged)
    Q_PROPERTY(QString defaultOutputLabel READ defaultOutputLabel NOTIFY
                   stateReprojected)
    Q_PROPERTY(QString defaultInputLabel READ defaultInputLabel NOTIFY
                   stateReprojected)
    Q_PROPERTY(bool hasDefaultOutput READ hasDefaultOutput NOTIFY
                   stateReprojected)
    Q_PROPERTY(bool hasDefaultInput READ hasDefaultInput NOTIFY
                   stateReprojected)
    Q_PROPERTY(int overflowDeviceCount READ overflowDeviceCount NOTIFY
                   stateReprojected)
    Q_PROPERTY(int overflowStreamCount READ overflowStreamCount NOTIFY
                   stateReprojected)
    Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
    explicit AudioAppletController(Audio::AudioClient *client,
                                   bool audioReadGranted,
                                   bool audioControlGranted,
                                   QObject *parent = nullptr);

    [[nodiscard]] QString phaseText() const;
    [[nodiscard]] QString phaseReasonText() const;
    [[nodiscard]] bool isControlGranted() const noexcept
    {
        return m_controlGranted;
    }
    [[nodiscard]] QVariantList deviceRows() const;
    [[nodiscard]] QVariantList streamRows() const;
    [[nodiscard]] QString defaultOutputLabel() const
    {
        return m_model.defaultOutputLabel();
    }
    [[nodiscard]] QString defaultInputLabel() const
    {
        return m_model.defaultInputLabel();
    }
    [[nodiscard]] bool hasDefaultOutput() const
    {
        return !m_model.defaultOutputLabel().isEmpty();
    }
    [[nodiscard]] bool hasDefaultInput() const
    {
        return !m_model.defaultInputLabel().isEmpty();
    }
    [[nodiscard]] int overflowDeviceCount() const
    {
        return m_model.overflowDeviceCount();
    }
    [[nodiscard]] int overflowStreamCount() const
    {
        return m_model.overflowStreamCount();
    }
    [[nodiscard]] bool feedbackPresent() const noexcept
    {
        return !m_feedback.isEmpty();
    }
    [[nodiscard]] QString feedback() const { return m_feedback; }

    // Requests are clamped and validated here against the current published
    // snapshot, then dispatched through the serialized client. A returned
    // false means the request was refused locally with feedback; it was never
    // dispatched. There is no automatic retry anywhere in this slice.
    Q_INVOKABLE bool requestVolume(quint64 serial, bool isStream,
                                   double volume);
    Q_INVOKABLE bool requestMute(quint64 serial, bool isStream, bool muted);
    // Console strip intents (ADR-0181), addressed by console id. Complete at
    // the service synchronously, so they carry no pending state here.
    Q_INVOKABLE bool requestStripFader(QString stripId, double position);
    Q_INVOKABLE bool requestStripMute(QString stripId, bool muted);
    [[nodiscard]] QVariantList consoleRows() const;
    [[nodiscard]] QVariantMap consoleLevels() const;
    Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
    void consoleLevelsChanged();
    void stateReprojected();
    void feedbackChanged();

private:
    enum class RequestKind { Volume, Mute };

    struct PendingRequest {
        quint64 requestId = 0;
        RequestKind kind = RequestKind::Volume;
    };

    // AGENT-CONTRACT: one request in flight per object and at most one queued
    // per object *and kind*, latest value wins (ADR-0191). A drag therefore
    // sends the value the finger is on when the previous request completes,
    // never a queue of stale intermediate values, and never a refusal. The
    // kind is part of the key so a mute is not starved behind a stream of
    // volume updates; when both are queued the mute goes first, because a user
    // who reaches for mute wants silence now.
    struct QueuedRequest {
        RequestKind kind = RequestKind::Volume;
        double volume = 0.0;
        bool muted = false;
        bool isStream = false;
    };

    // AGENT-CONTRACT: the value a control asked for, kept in the projection
    // until the service answers for it (ADR-0191). It is released once the
    // object has no outstanding work *and* a snapshot newer than the one the
    // request was made against has arrived, so a service that clamps or
    // refuses a value still takes the handle back rather than leaving it
    // parked on an intent that will never happen.
    struct RequestedState {
        RequestedValue value;
        quint64 initiatingEpoch = 0;
        quint64 initiatingRevision = 0;
    };

    void reproject();
    void prunePendingAgainstSnapshot();
    void publishFeedback(const QString &message);
    void handleOperationCompleted(quint64 requestId,
                                  const Audio::OperationResult &result);
    [[nodiscard]] bool
    beginRequest(quint64 serial, bool isStream, RequestKind kind, double volume,
                 bool muted);
    // The capability lookup and the client call. Re-checked on every dispatch,
    // including a dispatch out of the queue, because the snapshot may have
    // replaced the object since the value was queued.
    [[nodiscard]] bool dispatchRequest(quint64 serial, bool isStream,
                                       RequestKind kind, double volume,
                                       bool muted);
    void dispatchQueuedFor(quint64 serial);
    [[nodiscard]] bool hasOutstandingWork(quint64 serial) const;
    [[nodiscard]] QString
    requestFailureText(const Audio::OperationResult &result,
                       RequestKind kind) const;

    Audio::AudioClient *m_client = nullptr;
    AudioAppletModel m_model;
    QHash<quint64, PendingRequest> m_pendingBySerial;
    QHash<quint64, quint64> m_serialByRequestId;
    // Keyed by serial; each entry holds at most one queued volume and one
    // queued mute for that object.
    QHash<quint64, QueuedRequest> m_queuedVolumeBySerial;
    QHash<quint64, QueuedRequest> m_queuedMuteBySerial;
    QHash<quint64, RequestedState> m_requestedBySerial;
    QString m_feedback;
    bool m_readGranted = false;
    bool m_controlGranted = false;
};

} // namespace QindaQt::Shell::AudioApplet
