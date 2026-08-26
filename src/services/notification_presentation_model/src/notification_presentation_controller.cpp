// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"

#include "qindaqt/services/notification_presentation_client/notification_presentation_client.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace QindaQt::Services::NotificationPresentationModel {
namespace {

constexpr int MaximumPopupDurationMilliseconds = 60'000;

QVector<NotificationListEntry> activeEntries(
    const QVector<NotificationPresentation::PresentationNotification> &notifications)
{
    QVector<NotificationListEntry> result;
    result.reserve(notifications.size());
    for (const auto &notification : notifications) {
        result.append({notification, true});
    }
    return result;
}

} // namespace

bool PresentationTiming::isValid() const noexcept
{
    return lowUrgencyMilliseconds > 0 &&
        lowUrgencyMilliseconds <= MaximumPopupDurationMilliseconds &&
        normalUrgencyMilliseconds > 0 &&
        normalUrgencyMilliseconds <= MaximumPopupDurationMilliseconds &&
        criticalUrgencyMilliseconds > 0 &&
        criticalUrgencyMilliseconds <= MaximumPopupDurationMilliseconds &&
        operationErrorMilliseconds > 0 &&
        operationErrorMilliseconds <= MaximumPopupDurationMilliseconds &&
        maximumPopups > 0 && maximumPopups <= 32 && maximumHistory > 0 &&
        maximumHistory <= 1'000;
}

NotificationPresentationController::NotificationPresentationController(
    NotificationPresentationClient::NotificationPresentationClient &client,
    PresentationTiming timing, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_timing(timing.isValid() ? std::move(timing) : PresentationTiming{})
{
    m_clock.start();
    m_popupTimer.setSingleShot(true);
    m_operationErrorTimer.setSingleShot(true);
    connect(&m_popupTimer, &QTimer::timeout, this,
            &NotificationPresentationController::expirePopups);
    connect(&m_operationErrorTimer, &QTimer::timeout, this,
            [this] { setOperationError({}); });
    connect(&m_client,
            &NotificationPresentationClient::NotificationPresentationClient::
                stateChanged,
            this, &NotificationPresentationController::synchronize);
    connect(&m_client,
            &NotificationPresentationClient::NotificationPresentationClient::
                operationRejected,
            this, [this](quint32 notificationId, const QString &message) {
                m_pendingOperationId.reset();
                renewPopup(notificationId);
                setOperationError(message);
                Q_EMIT operationError(message);
            });
    connect(&m_client,
            &NotificationPresentationClient::NotificationPresentationClient::
                operationSucceeded,
            this, [this](quint32 notificationId) {
                m_pendingOperationId.reset();
                setOperationError({});
                removePopup(notificationId);
            });
    connect(&m_client,
            &NotificationPresentationClient::NotificationPresentationClient::
                operationInFlightChanged,
            this, [this] {
                if (operationBusy() || m_pendingOperationId) {
                    m_popupTimer.stop();
                } else {
                    rearmPopupTimer();
                }
                Q_EMIT operationBusyChanged();
            });
    synchronize();
}

QAbstractItemModel *NotificationPresentationController::activeModel() noexcept
{
    return &m_active;
}

QAbstractItemModel *NotificationPresentationController::popupModel() noexcept
{
    return &m_popups;
}

QAbstractItemModel *NotificationPresentationController::historyModel() noexcept
{
    return &m_history;
}

bool NotificationPresentationController::centerOpen() const noexcept
{
    return m_centerOpen;
}

int NotificationPresentationController::popupCount() const noexcept
{
    return m_popups.rowCount();
}

bool NotificationPresentationController::operationBusy() const noexcept
{
    return m_client.operationInFlight();
}

const QString &
NotificationPresentationController::operationErrorText() const noexcept
{
    return m_operationError;
}

void NotificationPresentationController::setCenterOpen(bool open)
{
    if (m_centerOpen == open) {
        return;
    }
    m_centerOpen = open;
    if (open && !m_popupEntries.isEmpty()) {
        m_popupEntries.clear();
        publishPopups();
    }
    if (!open) {
        rearmPopupTimer();
    }
    Q_EMIT centerOpenChanged();
}

void NotificationPresentationController::toggleCenter()
{
    setCenterOpen(!m_centerOpen);
}

void NotificationPresentationController::closePopup(quint32 notificationId)
{
    removePopup(notificationId);
}

void NotificationPresentationController::clearHistory()
{
    if (m_historyEntries.isEmpty()) {
        return;
    }
    m_historyEntries.clear();
    m_history.replace({});
}

bool NotificationPresentationController::dismiss(quint32 notificationId)
{
    // AGENT-GUARD: establish the pending identity before calling the client.
    // A test transport may complete synchronously from inside dismiss().
    setOperationError({});
    m_pendingOperationId = notificationId;
    m_popupTimer.stop();
    QString error;
    if (!m_client.dismiss(notificationId, &error)) {
        m_pendingOperationId.reset();
        setOperationError(error);
        Q_EMIT operationError(error);
        rearmPopupTimer();
        return false;
    }
    return true;
}

bool NotificationPresentationController::invokeAction(
    quint32 notificationId, const QString &actionKey)
{
    setOperationError({});
    m_pendingOperationId = notificationId;
    m_popupTimer.stop();
    QString error;
    // AGENT-NOTE: an empty activation token permits the action but cannot
    // promise focus transfer. Capability advertisement stays disabled until a
    // compositor/portal token source is integrated.
    if (!m_client.invokeAction(notificationId, actionKey, {}, &error)) {
        m_pendingOperationId.reset();
        setOperationError(error);
        Q_EMIT operationError(error);
        rearmPopupTimer();
        return false;
    }
    return true;
}

void NotificationPresentationController::synchronize()
{
    using State = NotificationPresentationClient::ClientState;
    if (m_client.state() != State::Ready || !m_client.snapshot()) {
        const bool hadPopups = !m_popupEntries.isEmpty();
        m_popupTimer.stop();
        m_active.replace({});
        m_popupEntries.clear();
        m_popups.replace({});
        m_previous.clear();
        m_epoch.clear();
        m_baselined = false;
        m_pendingOperationId.reset();
        if (hadPopups) {
            Q_EMIT popupCountChanged();
        }
        return;
    }
    const auto &snapshot = *m_client.snapshot();
    if (!m_baselined || snapshot.epoch != m_epoch) {
        baseline(snapshot);
        return;
    }
    update(snapshot);
}

void NotificationPresentationController::baseline(
    const NotificationPresentation::PresentationSnapshot &snapshot)
{
    const bool hadPopups = !m_popupEntries.isEmpty();
    m_popupTimer.stop();
    m_popupEntries.clear();
    m_popups.replace({});
    m_previous.clear();
    for (const auto &notification : snapshot.notifications) {
        m_previous.insert(notification.id, notification);
    }
    m_epoch = snapshot.epoch;
    m_baselined = true;
    m_active.replace(activeEntries(snapshot.notifications));
    if (hadPopups) {
        Q_EMIT popupCountChanged();
    }
}

void NotificationPresentationController::update(
    const NotificationPresentation::PresentationSnapshot &snapshot)
{
    QHash<quint32, NotificationPresentation::PresentationNotification> current;
    for (const auto &notification : snapshot.notifications) {
        current.insert(notification.id, notification);
    }

    auto removedIds = m_previous.keys();
    std::sort(removedIds.begin(), removedIds.end());
    for (const quint32 id : std::as_const(removedIds)) {
        if (current.contains(id)) {
            continue;
        }
        const auto removed = m_previous.value(id);
        if (!removed.transient) {
            addHistory(removed);
        }
        const auto popup = std::remove_if(
            m_popupEntries.begin(), m_popupEntries.end(),
            [id](const auto &entry) { return entry.notification.id == id; });
        m_popupEntries.erase(popup, m_popupEntries.end());
    }

    const qint64 now = m_clock.elapsed();
    for (const auto &notification : snapshot.notifications) {
        const auto previous = m_previous.constFind(notification.id);
        if (previous != m_previous.cend() && previous.value() == notification) {
            continue;
        }
        // AGENT-CONTRACT: the open center is already presenting current
        // notifications. Do not queue those same updates as stale popups that
        // appear only after the user closes the center.
        if (m_centerOpen) {
            continue;
        }
        const auto existing = std::find_if(
            m_popupEntries.begin(), m_popupEntries.end(),
            [&notification](const auto &entry) {
                return entry.notification.id == notification.id;
            });
        if (m_sequence == std::numeric_limits<quint64>::max()) {
            m_popupEntries.clear();
            m_sequence = 0;
        }
        PopupEntry replacement{notification,
                               now + popupDuration(notification.urgency),
                               ++m_sequence};
        if (existing == m_popupEntries.end()) {
            m_popupEntries.append(std::move(replacement));
        } else {
            *existing = std::move(replacement);
        }
    }
    while (m_popupEntries.size() > m_timing.maximumPopups) {
        const auto oldest = std::min_element(
            m_popupEntries.begin(), m_popupEntries.end(),
            [](const auto &left, const auto &right) {
                return left.sequence < right.sequence;
            });
        m_popupEntries.erase(oldest);
    }

    m_previous = std::move(current);
    m_active.replace(activeEntries(snapshot.notifications));
    publishPopups();
}

void NotificationPresentationController::publishPopups()
{
    const int previousCount = m_popups.rowCount();
    QVector<PopupEntry> ordered = m_popupEntries;
    std::sort(ordered.begin(), ordered.end(),
              [](const auto &left, const auto &right) {
                  if (left.notification.urgency != right.notification.urgency) {
                      return left.notification.urgency > right.notification.urgency;
                  }
                  return left.sequence > right.sequence;
              });
    QVector<NotificationListEntry> entries;
    entries.reserve(ordered.size());
    for (const auto &entry : std::as_const(ordered)) {
        entries.append({entry.notification, true});
    }
    m_popups.replace(std::move(entries));
    if (previousCount != m_popups.rowCount()) {
        Q_EMIT popupCountChanged();
    }
    rearmPopupTimer();
}

void NotificationPresentationController::expirePopups()
{
    const qint64 now = m_clock.elapsed();
    const auto expired = std::remove_if(
        m_popupEntries.begin(), m_popupEntries.end(),
        [now](const auto &entry) { return entry.deadlineMs <= now; });
    m_popupEntries.erase(expired, m_popupEntries.end());
    publishPopups();
}

void NotificationPresentationController::rearmPopupTimer()
{
    m_popupTimer.stop();
    if (m_popupEntries.isEmpty() || m_centerOpen || operationBusy() ||
        m_pendingOperationId) {
        return;
    }
    const auto earliest = std::min_element(
        m_popupEntries.cbegin(), m_popupEntries.cend(),
        [](const auto &left, const auto &right) {
            return left.deadlineMs < right.deadlineMs;
        });
    const qint64 remaining =
        std::max<qint64>(1, earliest->deadlineMs - m_clock.elapsed());
    m_popupTimer.start(int(std::min<qint64>(remaining,
                                           MaximumPopupDurationMilliseconds)));
}

void NotificationPresentationController::renewPopup(quint32 notificationId)
{
    const auto entry = std::find_if(
        m_popupEntries.begin(), m_popupEntries.end(),
        [notificationId](const auto &candidate) {
            return candidate.notification.id == notificationId;
        });
    if (entry != m_popupEntries.end()) {
        // AGENT-CONTRACT: a rejected operation must leave its originating card
        // actionable for a full retry interval, even if the old deadline elapsed
        // while the client awaited its asynchronous reply.
        entry->deadlineMs =
            m_clock.elapsed() + popupDuration(entry->notification.urgency);
        publishPopups();
        return;
    }
    rearmPopupTimer();
}

void NotificationPresentationController::removePopup(quint32 notificationId)
{
    const auto removed = std::remove_if(
        m_popupEntries.begin(), m_popupEntries.end(),
        [notificationId](const auto &entry) {
            return entry.notification.id == notificationId;
        });
    if (removed == m_popupEntries.end()) {
        return;
    }
    m_popupEntries.erase(removed, m_popupEntries.end());
    publishPopups();
}

void NotificationPresentationController::addHistory(
    const NotificationPresentation::PresentationNotification &notification)
{
    m_historyEntries.prepend({notification, false});
    if (m_historyEntries.size() > m_timing.maximumHistory) {
        m_historyEntries.resize(m_timing.maximumHistory);
    }
    m_history.replace(m_historyEntries);
}

void NotificationPresentationController::setOperationError(QString message)
{
    if (m_operationError == message) {
        if (!message.isEmpty()) {
            m_operationErrorTimer.start(m_timing.operationErrorMilliseconds);
        }
        return;
    }
    m_operationError = std::move(message);
    if (m_operationError.isEmpty()) {
        m_operationErrorTimer.stop();
    } else {
        m_operationErrorTimer.start(m_timing.operationErrorMilliseconds);
    }
    Q_EMIT operationErrorTextChanged();
}

int NotificationPresentationController::popupDuration(quint32 urgency) const noexcept
{
    if (urgency == 0) {
        return m_timing.lowUrgencyMilliseconds;
    }
    if (urgency == 2) {
        return m_timing.criticalUrgencyMilliseconds;
    }
    return m_timing.normalUrgencyMilliseconds;
}

} // namespace QindaQt::Services::NotificationPresentationModel
