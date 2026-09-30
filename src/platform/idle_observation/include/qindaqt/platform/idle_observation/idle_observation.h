// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QObject>
#include <functional>
#include <memory>
namespace QindaQt::Platform::Idle {
// Thread-confined readonly compositor idle state. Loss clears availability and
// idle; elapsed local time never manufactures compositor idle or unlocks.
class IdleObservation : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  virtual void setTimeout(int milliseconds) = 0;
  virtual bool available() const = 0;
  virtual bool idle() const = 0;
Q_SIGNALS:
  void changed();
};
// Owns each connected ordinary display FD transferred by opener, including
// failure paths. Captures in borrowed same-thread callbacks must outlive this
// object. lineageLive validates a public admitted attachment; revoke() follows
// its loss. No pathname reconnect, privileged locker FD or GUI dependency.
// Exactly one wl_seat and ext_idle_notifier_v1 global are supported; missing,
// duplicate or dynamically ambiguous globals revoke until explicit refresh.
class WaylandIdleObservation final : public IdleObservation {
  Q_OBJECT
public:
  WaylandIdleObservation(std::function<int()> opener,
                         std::function<bool()> lineageLive,
                         QObject *parent = nullptr);
  ~WaylandIdleObservation() override;
  // 0 disables; negative or >24h fails closed as disabled. Rearming starts
  // a fresh notification generation, discarding queued prior idle events.
  void setTimeout(int milliseconds) override;
  bool available() const override;
  bool idle() const override;
  void refresh();
  void revoke();

private:
  class Private;
  std::unique_ptr<Private> d;
};
} // namespace QindaQt::Platform::Idle
