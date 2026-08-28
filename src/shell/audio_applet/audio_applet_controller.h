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
// AGENT-NOTE: A pending entry whose serial disappears from the current
// snapshot is dropped from both maps without feedback; any late result for
// that request then arrives as an unknown request ID and is ignored. This is
// the deliberate bounded stale-cleanup path, not an operation replay.
class AudioAppletController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateReprojected)
    Q_PROPERTY(QString phaseReasonText READ phaseReasonText NOTIFY
                   stateReprojected)
    Q_PROPERTY(QVariantList deviceRows READ deviceRows NOTIFY stateReprojected)
    Q_PROPERTY(QVariantList streamRows READ streamRows NOTIFY stateReprojected)
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
                                   QObject *parent = nullptr);

    [[nodiscard]] QString phaseText() const;
    [[nodiscard]] QString phaseReasonText() const;
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
    Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
    void stateReprojected();
    void feedbackChanged();

private:
    enum class RequestKind { Volume, Mute };

    struct PendingRequest {
        quint64 requestId = 0;
        RequestKind kind = RequestKind::Volume;
    };

    void reproject();
    void prunePendingAgainstSnapshot();
    void publishFeedback(const QString &message);
    void handleOperationCompleted(quint64 requestId,
                                  const Audio::OperationResult &result);
    [[nodiscard]] bool
    beginRequest(quint64 serial, bool isStream, RequestKind kind, double volume,
                 bool muted);
    [[nodiscard]] QString
    requestFailureText(const Audio::OperationResult &result,
                       RequestKind kind) const;

    Audio::AudioClient *m_client = nullptr;
    AudioAppletModel m_model;
    QHash<quint64, PendingRequest> m_pendingBySerial;
    QHash<quint64, quint64> m_serialByRequestId;
    QString m_feedback;
};

} // namespace QindaQt::Shell::AudioApplet
