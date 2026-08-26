// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>
#include <QtTypes>

#include <functional>

namespace QindaQt::Services::NotificationHost {

enum class DeadlineArmStatus {
  Armed,
  InvalidRequest,
  Rejected,
};

struct DeadlineArmResult final {
  DeadlineArmStatus status = DeadlineArmStatus::InvalidRequest;
  QString message;

  [[nodiscard]] bool ok() const noexcept {
    return status == DeadlineArmStatus::Armed;
  }
};

// A single-shot, replace-on-arm scheduling seam. The scheduler and its event
// loop are confined to the host thread. A successful arm borrows no callback
// state: rearming or canceling must destroy the prior callback, and destruction
// must behave as cancel().
class NotificationDeadlineScheduler {
public:
  using Callback = std::function<void()>;

  virtual ~NotificationDeadlineScheduler() = default;

  // AGENT-CONTRACT: A successful arm queues callback for a later event-loop
  // turn; it must never invoke callback synchronously. Synchronous delivery
  // from modelPublished() would reenter NotificationService mutation.
  [[nodiscard]] virtual DeadlineArmResult armAfter(qint64 delayMs,
                                                   Callback callback) = 0;
  virtual void cancel() noexcept = 0;
};

} // namespace QindaQt::Services::NotificationHost
