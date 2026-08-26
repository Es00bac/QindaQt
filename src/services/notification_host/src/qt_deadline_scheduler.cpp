// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/notification_host/qt_deadline_scheduler.h"

#include <limits>
#include <utility>

namespace QindaQt::Services::NotificationHost {

QtNotificationDeadlineScheduler::QtNotificationDeadlineScheduler(
    QObject *parent)
    : QObject(parent) {
  m_timer.setSingleShot(true);
  QObject::connect(&m_timer, &QTimer::timeout, this,
                   [this] { deadlineReached(); });
}

QtNotificationDeadlineScheduler::~QtNotificationDeadlineScheduler() {
  cancel();
}

DeadlineArmResult QtNotificationDeadlineScheduler::armAfter(qint64 delayMs,
                                                            Callback callback) {
  if (delayMs < 0 || delayMs > std::numeric_limits<int>::max()) {
    return {
        DeadlineArmStatus::InvalidRequest,
        QStringLiteral("notification deadline delay is outside QTimer range"),
    };
  }
  if (!callback) {
    return {
        DeadlineArmStatus::InvalidRequest,
        QStringLiteral("notification deadline callback is empty"),
    };
  }

  m_timer.stop();
  m_callback = std::move(callback);
  m_timer.start(int(delayMs));
  return {DeadlineArmStatus::Armed, {}};
}

void QtNotificationDeadlineScheduler::cancel() noexcept {
  m_timer.stop();
  m_callback = {};
}

void QtNotificationDeadlineScheduler::deadlineReached() {
  auto callback = std::move(m_callback);
  m_callback = {};
  if (callback) {
    callback();
  }
}

} // namespace QindaQt::Services::NotificationHost
