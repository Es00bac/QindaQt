// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/power_client/power_client.h>
#include <qindaqt/shell/power_applet/brightness_request_state.h>
#include <qindaqt/shell/power_applet/power_applet_types.h>

#include <QtCore/QObject>
#include <QtCore/QVariantList>

namespace QindaQt::Shell::PowerApplet {

// Shell-private adapter from the exact-owner PowerClient to bounded values
// consumable by compiled panel QML. Presentation never sees transport objects,
// wire handles, or a service lookup.
//
// AGENT-CONTRACT: The borrowed client must outlive this controller and share
// its thread. Shell composition owns client start/stop. This adapter never
// retries an operation: owner loss and uncertain completion end the local
// request and require a newly observed snapshot plus a new user gesture.
class PowerAppletController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY stateChanged)
    Q_PROPERTY(QString diagnostic READ diagnostic NOTIFY stateChanged)
    Q_PROPERTY(bool hasBattery READ hasBattery NOTIFY stateChanged)
    Q_PROPERTY(QString batteryLabel READ batteryLabel NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
    Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY stateChanged)
    Q_PROPERTY(QVariantList profileRows READ profileRows NOTIFY stateChanged)
    Q_PROPERTY(QVariantList keyboardRows READ keyboardRows NOTIFY stateChanged)
    Q_PROPERTY(bool operationPending READ operationPending NOTIFY stateChanged)
    Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
    explicit PowerAppletController(Power::PowerClient *client,
                                   bool powerReadGranted,
                                   bool powerControlGranted,
                                   QObject *parent = nullptr);

    [[nodiscard]] QString phase() const;
    [[nodiscard]] QString diagnostic() const { return m_model.diagnostic; }
    [[nodiscard]] bool hasBattery() const noexcept { return m_model.summary.present; }
    [[nodiscard]] QString batteryLabel() const;
    [[nodiscard]] QString accessibleName() const;
    [[nodiscard]] QString accessibleDescription() const;
    [[nodiscard]] QVariantList profileRows() const;
    [[nodiscard]] QVariantList keyboardRows() const;
    [[nodiscard]] bool operationPending() const noexcept;
    [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
    [[nodiscard]] QString feedback() const { return m_feedback; }

    // `normalized` uses Power1's exact 0..10000 scale. The controller resolves
    // the opaque ID only within the current snapshot, converts using the
    // device's current raw bound, and dispatches at most once.
    Q_INVOKABLE bool requestKeyboardBrightness(const QString &controlId,
                                               int normalized);
    Q_INVOKABLE bool requestProfile(const QString &profileId);
    Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
    void stateChanged();
    void feedbackChanged();

private:
    enum class PendingKind {
        None,
        KeyboardBrightness,
        Profile,
    };

    void reproject();
    void handleOperationCompleted(quint64 requestId,
                                  const Power::OperationResult &result);
    void publishFeedback(const QString &message);
    [[nodiscard]] bool presentationOwnerAvailable() const noexcept;
    [[nodiscard]] QString profileResultFeedback(
        const Power::OperationResult &result) const;

    Power::PowerClient *m_client = nullptr;
    bool m_powerReadGranted = false;
    bool m_powerControlGranted = false;
    PowerAppletModel m_model;
    BrightnessRequest m_request;
    PendingKind m_pendingKind = PendingKind::None;
    QString m_pendingProfileId;
    quint64 m_pendingEpoch = 0;
    quint64 m_pendingRevision = 0;
    quint64 m_requestId = 0;
    QString m_feedback;
};

} // namespace QindaQt::Shell::PowerApplet
