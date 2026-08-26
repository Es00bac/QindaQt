// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/notification_host/deadline_scheduler.h"

#include <QObject>
#include <QTimer>

namespace QindaQt::Services::NotificationHost {

class QtNotificationDeadlineScheduler final
    : public QObject,
      public NotificationDeadlineScheduler {
public:
  explicit QtNotificationDeadlineScheduler(QObject *parent = nullptr);
  ~QtNotificationDeadlineScheduler() override;

  [[nodiscard]] DeadlineArmResult armAfter(qint64 delayMs,
                                           Callback callback) override;
  void cancel() noexcept override;

private:
  void deadlineReached();

  QTimer m_timer;
  Callback m_callback;
};

} // namespace QindaQt::Services::NotificationHost
