// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QList>
#include <QObject>
#include <QTimer>
#include <chrono>
#include <functional>
#include <memory>
namespace QindaQt::Platform::Recovery {
// AGENT-NOTE: shared bounded-retry primitive (ADR-0379). A component that
// attaches to a peer, holds a subscription or waits for a session resource
// retries through this schedule instead of polling on a fixed short interval
// or giving up after one failure. See architecture/session-resilience.md.

// One-shot timer port. Production uses QtRecoveryTimer; deterministic tests
// inject ManualRecoveryTimer and fire it explicitly instead of sleeping.
// AGENT-CONTRACT: same-thread QObject; start() replaces a pending shot.
class RecoveryTimer : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  ~RecoveryTimer() override = default;
  virtual void start(std::chrono::milliseconds delay) = 0;
  virtual void stop() = 0;
  [[nodiscard]] virtual bool isActive() const = 0;
Q_SIGNALS:
  void timeout();
};

class QtRecoveryTimer final : public RecoveryTimer {
  Q_OBJECT
public:
  explicit QtRecoveryTimer(QObject *parent = nullptr);
  void start(std::chrono::milliseconds delay) override;
  void stop() override;
  [[nodiscard]] bool isActive() const override;

private:
  QTimer m_timer;
};

// Deterministic test double: records each requested delay and elapses only
// when fire() is called.
class ManualRecoveryTimer final : public RecoveryTimer {
  Q_OBJECT
public:
  using RecoveryTimer::RecoveryTimer;
  void start(std::chrono::milliseconds delay) override;
  void stop() override;
  [[nodiscard]] bool isActive() const override;
  // Elapses the pending shot synchronously; false when none is pending.
  bool fire();
  [[nodiscard]] const QList<std::chrono::milliseconds> &requested() const {
    return m_requested;
  }

private:
  QList<std::chrono::milliseconds> m_requested;
  bool m_active = false;
};

// initial * 2^attempt, saturating at maximum. Defaults follow the stability
// round rule: 0.25 s doubling to 30 s.
struct BackoffPolicy final {
  std::chrono::milliseconds initial{250};
  std::chrono::milliseconds maximum{30'000};
  [[nodiscard]] std::chrono::milliseconds delayFor(int attempt) const noexcept;
};

// Factory seam for components that own one or more schedules. An empty
// factory selects QtRecoveryTimer.
using RecoveryTimerFactory = std::function<std::unique_ptr<RecoveryTimer>()>;

// Bounded exponential retry schedule over one owned timer. schedule() arms at
// most one pending retry; reset() after success restarts from the initial
// delay. The retry signal is the only callback and may reenter schedule().
class RecoveryBackoff final : public QObject {
  Q_OBJECT
public:
  explicit RecoveryBackoff(BackoffPolicy policy = {},
                           const RecoveryTimerFactory &timers = {},
                           QObject *parent = nullptr);
  ~RecoveryBackoff() override;
  // Arms the next retry unless one is pending; returns the armed delay.
  std::chrono::milliseconds schedule();
  void reset();
  void cancel();
  [[nodiscard]] bool pending() const;
  [[nodiscard]] int attempts() const noexcept { return m_attempts; }
  [[nodiscard]] const BackoffPolicy &policy() const noexcept { return m_policy; }
Q_SIGNALS:
  void retry();

private:
  BackoffPolicy m_policy;
  std::unique_ptr<RecoveryTimer> m_timer;
  std::chrono::milliseconds m_armed{0};
  int m_attempts = 0;
};
} // namespace QindaQt::Platform::Recovery
