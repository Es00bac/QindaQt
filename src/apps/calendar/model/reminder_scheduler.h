// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "calendar_types.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QtTypes>

#include <functional>
#include <memory>

class QTimer;

namespace QindaQt::Apps::Calendar {

// Single-shot, replace-on-arm scheduling seam, mirroring
// Services::NotificationHost::NotificationDeadlineScheduler. The scheduler
// and its timer live on one thread with an event loop. A successful arm must
// queue the callback for a later event-loop turn, never invoke it
// synchronously; rearming or canceling destroys the prior callback, and
// destruction must behave as cancel().
class ReminderTimer {
public:
  using Callback = std::function<void()>;

  virtual ~ReminderTimer() = default;

  virtual void armAfter(qint64 delayMs, Callback callback) = 0;
  virtual void cancel() noexcept = 0;
};

// Production ReminderTimer backed by a single-shot QTimer.
class QTimerReminderTimer final : public ReminderTimer {
public:
  QTimerReminderTimer();
  ~QTimerReminderTimer() override;

  void armAfter(qint64 delayMs, Callback callback) override;
  void cancel() noexcept override;

private:
  // Held via pointer because QTimer is a QObject; the unique_ptr keeps this
  // class copy-free while staying parentless.
  std::unique_ptr<QTimer> m_timer;
  Callback m_callback;
};

// Schedules VALARM-derived reminders against an injected wall clock. Pure
// in-process signaling: it emits reminderDue() and knows nothing about the
// OS notification server (wired up by the composition root in a later chunk).
class ReminderScheduler final : public QObject {
  Q_OBJECT

public:
  // Wall-clock now provider; tests inject a controllable value.
  using Clock = std::function<QDateTime()>;

  // Both dependencies are constructor-visible. The scheduler takes ownership
  // of the timer; production passes a QTimerReminderTimer, tests a manual
  // implementation they fire by hand.
  ReminderScheduler(Clock clock, std::unique_ptr<ReminderTimer> timer,
                    QObject *parent = nullptr);
  ~ReminderScheduler() override;

  // Replaces the entire pending set. Identical requests are deduplicated, and
  // requests whose fireAt is not in the future are dropped without firing — a
  // restarted app must not replay reminders that came due while it was off.
  void reschedule(const QList<ReminderRequest> &requests);

  // Pending set after filtering/deduplication, sorted by fireAt. Exposed for
  // diagnostics and tests.
  [[nodiscard]] QList<ReminderRequest> pendingRequests() const;

signals:
  void reminderDue(const QString &eventUid, const QString &summary,
                   const QDateTime &occurrenceStart);

private:
  void armNext();
  void fireDueReminders();

  Clock m_clock;
  std::unique_ptr<ReminderTimer> m_timer;
  QList<ReminderRequest> m_pending;
};

} // namespace QindaQt::Apps::Calendar
