// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation_model/notification_list_model.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <optional>

namespace QindaQt::Services::NotificationPresentationClient {
class NotificationPresentationClient;
}

namespace QindaQt::Services::NotificationPresentationPolicy {
class NotificationInterruptionPolicy;
class NotificationPrivacyPolicy;
}

namespace QindaQt::Services::NotificationPresentationModel {

struct PresentationTiming final {
    int lowUrgencyMilliseconds = 4'000;
    int normalUrgencyMilliseconds = 6'000;
    int criticalUrgencyMilliseconds = 10'000;
    int operationErrorMilliseconds = 8'000;
    qsizetype maximumPopups = 8;
    qsizetype maximumHistory = 100;

    [[nodiscard]] bool isValid() const noexcept;
};

class NotificationPresentationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractItemModel *activeModel READ activeModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *popupModel READ popupModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel *historyModel READ historyModel CONSTANT)
    Q_PROPERTY(bool centerOpen READ centerOpen WRITE setCenterOpen
                   NOTIFY centerOpenChanged)
    Q_PROPERTY(bool doNotDisturbEnabled READ doNotDisturbEnabled
                   WRITE setDoNotDisturbEnabled
                       NOTIFY doNotDisturbEnabledChanged)
    Q_PROPERTY(bool privatePresentationAllowed READ privatePresentationAllowed
                   NOTIFY privatePresentationAllowedChanged)
    Q_PROPERTY(int popupCount READ popupCount NOTIFY popupCountChanged)
    Q_PROPERTY(bool operationBusy READ operationBusy NOTIFY operationBusyChanged)
    Q_PROPERTY(QString operationErrorText READ operationErrorText
                   NOTIFY operationErrorTextChanged)

public:
    // AGENT-CONTRACT: `client`, `interruptionPolicy`, and `privacyPolicy` are
    // borrowed, must remain on this object's thread, and must outlive the
    // controller. Invalid timing is replaced by bounded defaults instead of
    // weakening limits. Privacy starts denied and outranks interruption
    // policy, urgency, operations, and every public presentation projection.
    explicit NotificationPresentationController(
        NotificationPresentationClient::NotificationPresentationClient &client,
        NotificationPresentationPolicy::NotificationInterruptionPolicy &
            interruptionPolicy,
        NotificationPresentationPolicy::NotificationPrivacyPolicy &privacyPolicy,
        PresentationTiming timing = {}, QObject *parent = nullptr);

    [[nodiscard]] QAbstractItemModel *activeModel() noexcept;
    [[nodiscard]] QAbstractItemModel *popupModel() noexcept;
    [[nodiscard]] QAbstractItemModel *historyModel() noexcept;
    [[nodiscard]] bool centerOpen() const noexcept;
    [[nodiscard]] bool doNotDisturbEnabled() const noexcept;
    [[nodiscard]] bool privatePresentationAllowed() const noexcept;
    [[nodiscard]] int popupCount() const noexcept;
    [[nodiscard]] bool operationBusy() const noexcept;
    [[nodiscard]] const QString &operationErrorText() const noexcept;

    void setCenterOpen(bool open);
    void setDoNotDisturbEnabled(bool enabled);
    Q_INVOKABLE void toggleCenter();
    Q_INVOKABLE void closePopup(quint32 notificationId);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE bool dismiss(quint32 notificationId);
    Q_INVOKABLE bool invokeAction(quint32 notificationId,
                                  const QString &actionKey);

Q_SIGNALS:
    void centerOpenChanged();
    void doNotDisturbEnabledChanged();
    void privatePresentationAllowedChanged();
    void popupCountChanged();
    void operationBusyChanged();
    void operationErrorTextChanged();
    void operationError(const QString &message);

private:
    struct PopupEntry final {
        NotificationPresentation::PresentationNotification notification;
        qint64 deadlineMs = 0;
        quint64 sequence = 0;
    };

    void synchronize();
    void handleInterruptionPolicyChanged();
    void handlePrivacyPolicyChanged();
    void clearPrivatePresentation();
    void baseline(const NotificationPresentation::PresentationSnapshot &snapshot);
    void update(const NotificationPresentation::PresentationSnapshot &snapshot);
    void publishPopups();
    void expirePopups();
    void rearmPopupTimer();
    void removePopup(quint32 notificationId);
    void renewPopup(quint32 notificationId);
    void addHistory(
        const NotificationPresentation::PresentationNotification &notification);
    void setOperationError(QString message);
    [[nodiscard]] int popupDuration(quint32 urgency) const noexcept;

    NotificationPresentationClient::NotificationPresentationClient &m_client;
    NotificationPresentationPolicy::NotificationInterruptionPolicy &
        m_interruptionPolicy;
    NotificationPresentationPolicy::NotificationPrivacyPolicy &m_privacyPolicy;
    PresentationTiming m_timing;
    NotificationListModel m_active;
    NotificationListModel m_popups;
    NotificationListModel m_history;
    QHash<quint32, NotificationPresentation::PresentationNotification> m_previous;
    QVector<PopupEntry> m_popupEntries;
    QVector<NotificationListEntry> m_historyEntries;
    QTimer m_popupTimer;
    QTimer m_operationErrorTimer;
    QElapsedTimer m_clock;
    QString m_epoch;
    QString m_operationError;
    std::optional<quint32> m_pendingOperationId;
    quint64 m_sequence = 0;
    bool m_baselined = false;
    bool m_centerOpen = false;
    bool m_suppressCurrentOperationOutcome = false;
};

} // namespace QindaQt::Services::NotificationPresentationModel
