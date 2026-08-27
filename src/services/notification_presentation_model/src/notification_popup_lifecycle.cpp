// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/services/notification_presentation_model/notification_presentation_controller.h"

#include <algorithm>
#include <utility>

namespace QindaQt::Services::NotificationPresentationModel {
namespace {

constexpr int MaximumPopupDurationMilliseconds = 60'000;

} // namespace

void NotificationPresentationController::publishPopups()
{
    if (!privatePresentationAllowed()) {
        clearPrivatePresentation();
        return;
    }
    const int previousCount = m_popups.rowCount();
    QVector<PopupEntry> ordered = m_popupEntries;
    std::sort(ordered.begin(), ordered.end(),
              [](const auto &left, const auto &right) {
                  if (left.notification.urgency != right.notification.urgency) {
                      return left.notification.urgency >
                          right.notification.urgency;
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
    if (!privatePresentationAllowed() || m_popupEntries.isEmpty() ||
        m_centerOpen || operationBusy() || m_pendingOperationId) {
        return;
    }
    const auto earliest =
        std::min_element(m_popupEntries.cbegin(), m_popupEntries.cend(),
                         [](const auto &left, const auto &right) {
                             return left.deadlineMs < right.deadlineMs;
                         });
    const qint64 remaining =
        std::max<qint64>(1, earliest->deadlineMs - m_clock.elapsed());
    m_popupTimer.start(
        int(std::min<qint64>(remaining, MaximumPopupDurationMilliseconds)));
}

void NotificationPresentationController::renewPopup(quint32 notificationId)
{
    const auto entry =
        std::find_if(m_popupEntries.begin(), m_popupEntries.end(),
                     [notificationId](const auto &candidate) {
                         return candidate.notification.id == notificationId;
                     });
    if (entry != m_popupEntries.end()) {
        // AGENT-CONTRACT: a rejected operation must leave its originating card
        // actionable for a full retry interval, even if the old deadline
        // elapsed while the client awaited its asynchronous reply.
        entry->deadlineMs =
            m_clock.elapsed() + popupDuration(entry->notification.urgency);
        publishPopups();
        return;
    }
    rearmPopupTimer();
}

void NotificationPresentationController::removePopup(quint32 notificationId)
{
    const auto removed =
        std::remove_if(m_popupEntries.begin(), m_popupEntries.end(),
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

int NotificationPresentationController::popupDuration(
    quint32 urgency) const noexcept
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
