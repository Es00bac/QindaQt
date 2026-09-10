// SPDX-License-Identifier: GPL-3.0-or-later
#include "reminder_scheduler.h"

#include <QTimer>

#include <algorithm>
#include <limits>

namespace QindaQt::Apps::Calendar {

QTimerReminderTimer::QTimerReminderTimer() : m_timer(std::make_unique<QTimer>()) {
  m_timer->setSingleShot(true);
  QObject::connect(m_timer.get(), &QTimer::timeout, m_timer.get(), [this] {
    Callback callback = std::move(m_callback);
    m_callback = nullptr;
    if (callback) {
      callback();
    }
  });
}

QTimerReminderTimer::~QTimerReminderTimer() { cancel(); }

void QTimerReminderTimer::armAfter(const qint64 delayMs, Callback callback) {
  m_callback = std::move(callback);
  // QTimer takes a signed int of milliseconds (~24 days). Oversized delays
  // are clamped; the scheduler re-arms for the remainder when the clamped
  // shot fires and finds nothing due.
  const qint64 clamped =
      std::clamp<qint64>(delayMs, 0, std::numeric_limits<int>::max());
  m_timer->start(static_cast<int>(clamped));
}

void QTimerReminderTimer::cancel() noexcept {
  m_timer->stop();
  m_callback = nullptr;
}

ReminderScheduler::ReminderScheduler(Clock clock,
                                     std::unique_ptr<ReminderTimer> timer,
                                     QObject *parent)
    : QObject(parent), m_clock(std::move(clock)), m_timer(std::move(timer)) {}

ReminderScheduler::~ReminderScheduler() {
  // AGENT-GUARD: The armed callback captures this scheduler; canceling before
  // the timer member dies keeps a late timer fire from reentering a
  // half-destroyed object.
  m_timer->cancel();
}

void ReminderScheduler::reschedule(const QList<ReminderRequest> &requests) {
  const QDateTime now = m_clock();
  QList<ReminderRequest> pending;
  for (const ReminderRequest &request : requests) {
    if (!request.fireAt.isValid() || !request.occurrenceStart.isValid() ||
        request.eventUid.isEmpty() || request.fireAt <= now) {
      continue;
    }
    if (!pending.contains(request)) {
      pending.append(request);
    }
  }
  std::sort(pending.begin(), pending.end(),
            [](const ReminderRequest &left, const ReminderRequest &right) {
              if (left.fireAt != right.fireAt) {
                return left.fireAt < right.fireAt;
              }
              if (left.eventUid != right.eventUid) {
                return left.eventUid < right.eventUid;
              }
              return left.occurrenceStart < right.occurrenceStart;
            });
  m_pending = pending;
  m_timer->cancel();
  armNext();
}

QList<ReminderRequest> ReminderScheduler::pendingRequests() const {
  return m_pending;
}

void ReminderScheduler::armNext() {
  if (m_pending.isEmpty()) {
    return;
  }
  const qint64 delayMs = m_clock().msecsTo(m_pending.first().fireAt);
  m_timer->armAfter(delayMs, [this] { fireDueReminders(); });
}

void ReminderScheduler::fireDueReminders() {
  const QDateTime now = m_clock();
  while (!m_pending.isEmpty() && m_pending.first().fireAt <= now) {
    const ReminderRequest due = m_pending.takeFirst();
    emit reminderDue(due.eventUid, due.summary, due.occurrenceStart);
  }
  armNext();
}

} // namespace QindaQt::Apps::Calendar
