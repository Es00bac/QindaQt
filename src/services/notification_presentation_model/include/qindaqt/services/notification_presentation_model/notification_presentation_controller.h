// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_presentation_model/notification_list_model.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

namespace QindaQt::Services::NotificationPresentationClient {
class NotificationPresentationClient;
}

namespace QindaQt::Services::NotificationPresentationModel {

struct PresentationTiming final {
    int lowUrgencyMilliseconds = 4'000;
    int normalUrgencyMilliseconds = 6'000;
    int criticalUrgencyMilliseconds = 10'000;
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
    Q_PROPERTY(int popupCount READ popupCount NOTIFY popupCountChanged)

public:
    // AGENT-CONTRACT: `client` is borrowed, must remain on this object's
    // thread, and must outlive the controller. Invalid timing is replaced by
    // the bounded defaults instead of weakening presentation limits.
    explicit NotificationPresentationController(
        NotificationPresentationClient::NotificationPresentationClient &client,
        PresentationTiming timing = {}, QObject *parent = nullptr);

    [[nodiscard]] QAbstractItemModel *activeModel() noexcept;
    [[nodiscard]] QAbstractItemModel *popupModel() noexcept;
    [[nodiscard]] QAbstractItemModel *historyModel() noexcept;
    [[nodiscard]] bool centerOpen() const noexcept;
    [[nodiscard]] int popupCount() const noexcept;

    void setCenterOpen(bool open);
    Q_INVOKABLE void toggleCenter();
    Q_INVOKABLE void closePopup(quint32 notificationId);
    Q_INVOKABLE void clearHistory();
    Q_INVOKABLE bool dismiss(quint32 notificationId);
    Q_INVOKABLE bool invokeAction(quint32 notificationId,
                                  const QString &actionKey);

Q_SIGNALS:
    void centerOpenChanged();
    void popupCountChanged();
    void operationError(const QString &message);

private:
    struct PopupEntry final {
        NotificationPresentation::PresentationNotification notification;
        qint64 deadlineMs = 0;
        quint64 sequence = 0;
    };

    void synchronize();
    void baseline(const NotificationPresentation::PresentationSnapshot &snapshot);
    void update(const NotificationPresentation::PresentationSnapshot &snapshot);
    void publishPopups();
    void expirePopups();
    void rearmPopupTimer();
    void removePopup(quint32 notificationId);
    void addHistory(
        const NotificationPresentation::PresentationNotification &notification);
    [[nodiscard]] int popupDuration(quint32 urgency) const noexcept;

    NotificationPresentationClient::NotificationPresentationClient &m_client;
    PresentationTiming m_timing;
    NotificationListModel m_active;
    NotificationListModel m_popups;
    NotificationListModel m_history;
    QHash<quint32, NotificationPresentation::PresentationNotification> m_previous;
    QVector<PopupEntry> m_popupEntries;
    QVector<NotificationListEntry> m_historyEntries;
    QTimer m_popupTimer;
    QElapsedTimer m_clock;
    QString m_epoch;
    quint64 m_sequence = 0;
    bool m_baselined = false;
    bool m_centerOpen = false;
};

} // namespace QindaQt::Services::NotificationPresentationModel
