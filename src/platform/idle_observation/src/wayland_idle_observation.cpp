// SPDX-License-Identifier: GPL-3.0-or-later
#include "ext-idle-notify-client.h"
#include "qindaqt/platform/idle_observation/idle_observation.h"
#include <QSocketNotifier>
#include <QTimer>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <wayland-client.h>
namespace QindaQt::Platform::Idle {
class WaylandIdleObservation::Private {
public:
  struct NotificationContext {
    Private *self;
    quint64 serial;
  };
  Private(WaylandIdleObservation &object, std::function<int()> acquire,
          std::function<bool()> admitted)
      : q(object), opener(std::move(acquire)),
        lineageLive(std::move(admitted)) {
    deadline.setSingleShot(true);
    deadline.setInterval(2000);
    QObject::connect(&deadline, &QTimer::timeout, &q, [this] { clear(); });
  }
  ~Private() { clear(false); }
  void clear(bool publish = true) {
    ++generation;
    ++notificationGeneration;
    ready = false;
    isIdle = false;
    invalidated = true;
    deadline.stop();
    if (dispatching) {
      const auto serial = generation;
      QTimer::singleShot(0, &q, [this, serial] {
        if (serial == generation)
          teardown();
      });
    } else
      teardown();
    if (publish)
      Q_EMIT q.changed();
  }
  void teardown() {
    reading.reset();
    writing.reset();
    if (notification)
      ext_idle_notification_v1_destroy(notification);
    notification = nullptr;
    if (sync)
      wl_callback_destroy(sync);
    sync = nullptr;
    if (manager)
      ext_idle_notifier_v1_destroy(manager);
    manager = nullptr;
    if (seat)
      wl_seat_destroy(seat);
    seat = nullptr;
    if (registry)
      wl_registry_destroy(registry);
    registry = nullptr;
    if (display)
      wl_display_disconnect(display);
    display = nullptr;
    contexts.clear();
    seats = 0;
    notifiers = 0;
    seatName = 0;
    managerName = 0;
  }
  bool admitted() const { return lineageLive && lineageLive(); }
  void flush() {
    if (!display || invalidated)
      return;
    if (wl_display_flush(display) < 0) {
      if (errno != EAGAIN) {
        clear();
        return;
      }
      writing->setEnabled(true);
    } else
      writing->setEnabled(false);
  }
  void dispatch() {
    if (!display || dispatching)
      return;
    if (invalidated || !admitted()) {
      clear();
      return;
    }
    while (wl_display_prepare_read(display) != 0) {
      dispatching = true;
      const auto pending = wl_display_dispatch_pending(display);
      dispatching = false;
      if (pending < 0) {
        clear();
        return;
      }
      if (invalidated) {
        teardown();
        return;
      }
    }
    if (wl_display_read_events(display) < 0) {
      clear();
      return;
    }
    dispatching = true;
    const auto result = wl_display_dispatch_pending(display);
    dispatching = false;
    if (result < 0 || !admitted()) {
      clear();
      return;
    }
    if (invalidated) {
      teardown();
      return;
    }
    flush();
  }
  static void notificationEvent(void *data, ext_idle_notification_v1 *source,
                                bool value) {
    const auto &context = *static_cast<NotificationContext *>(data);
    auto &self = *context.self;
    // AGENT-GUARD: notification events queued before rearm cannot grant a
    // fresh generation idle, including reentrant timeout changes (ADR-0307).
    if (context.serial != self.notificationGeneration ||
        source != self.notification || self.invalidated || !self.ready)
      return;
    if (!self.admitted()) {
      self.clear();
      return;
    }
    self.isIdle = value;
    if (!value) Q_EMIT self.q.activity();
    if (context.serial != self.notificationGeneration ||
        source != self.notification || self.invalidated || !self.ready) return;
    Q_EMIT self.q.changed();
  }
  void arm() {
    ++notificationGeneration;
    if (notification)
      ext_idle_notification_v1_destroy(notification);
    notification = nullptr;
    isIdle = false;
    if (invalidated || !ready || !timeout || !manager || !seat || !admitted()) {
      Q_EMIT q.changed();
      return;
    }
    // Retain bounded listener identities for this connection. Rotate through
    // the admitted ordinary opener before repeated edits can grow them.
    if (contexts.size() >= 64) {
      open();
      return;
    }
    notification = ext_idle_notifier_v1_get_idle_notification(
        manager, static_cast<uint32_t>(timeout), seat);
    if (!notification) {
      clear();
      return;
    }
    auto context = std::make_unique<NotificationContext>(
        NotificationContext{this, notificationGeneration});
    static const ext_idle_notification_v1_listener events{
        [](void *data, ext_idle_notification_v1 *source) {
          notificationEvent(data, source, true);
        },
        [](void *data, ext_idle_notification_v1 *source) {
          notificationEvent(data, source, false);
        }};
    ext_idle_notification_v1_add_listener(notification, &events, context.get());
    contexts.push_back(std::move(context));
    flush();
    Q_EMIT q.changed();
  }
  void open() {
    if (dispatching) {
      clear();
      const auto serial = generation;
      QTimer::singleShot(0, &q, [this, serial] {
        if (serial == generation)
          open();
      });
      return;
    }
    clear();
    if (!timeout || !opener || !admitted())
      return;
    const int fd = opener();
    if (fd < 0)
      return;
    if (!admitted()) {
      close(fd);
      return;
    }
    display = wl_display_connect_to_fd(fd);
    // AGENT-CONTRACT: libwayland takes fd ownership even when connection
    // construction fails; closing here can close an unrelated reused FD.
    if (!display)
      return;
    invalidated = false;
    registry = wl_display_get_registry(display);
    static const wl_registry_listener globals{
        [](void *data, wl_registry *r, uint32_t name, const char *interface,
           uint32_t version) {
          auto &self = *static_cast<Private *>(data);
          if (self.invalidated)
            return;
          if (std::strcmp(interface, wl_seat_interface.name) == 0) {
            if (++self.seats != 1 || version < 1) {
              self.clear();
              return;
            }
            self.seat = static_cast<wl_seat *>(
                wl_registry_bind(r, name, &wl_seat_interface, 1));
            self.seatName = name;
            static const wl_seat_listener ignored{
                [](void *, wl_seat *, uint32_t) {},
                [](void *, wl_seat *, const char *) {}};
            wl_seat_add_listener(self.seat, &ignored, &self);
          } else if (std::strcmp(interface,
                                 ext_idle_notifier_v1_interface.name) == 0) {
            if (++self.notifiers != 1 || version < 1) {
              self.clear();
              return;
            }
            self.manager = static_cast<ext_idle_notifier_v1 *>(
                wl_registry_bind(r, name, &ext_idle_notifier_v1_interface, 1));
            self.managerName = name;
          }
        },
        [](void *data, wl_registry *, uint32_t name) {
          auto &self = *static_cast<Private *>(data);
          if (name == self.seatName || name == self.managerName)
            self.clear();
        }};
    wl_registry_add_listener(registry, &globals, this);
    sync = wl_display_sync(display);
    static const wl_callback_listener completed{
        [](void *data, wl_callback *callback, uint32_t) {
          auto &self = *static_cast<Private *>(data);
          wl_callback_destroy(callback);
          self.sync = nullptr;
          self.deadline.stop();
          if (self.invalidated)
            return;
          if (self.seats != 1 || self.notifiers != 1 || !self.manager ||
              !self.seat || !self.admitted()) {
            self.clear();
            return;
          }
          self.ready = true;
          self.arm();
        }};
    wl_callback_add_listener(sync, &completed, this);
    const int descriptor = wl_display_get_fd(display);
    reading = std::make_unique<QSocketNotifier>(descriptor,
                                                QSocketNotifier::Read, &q);
    writing = std::make_unique<QSocketNotifier>(descriptor,
                                                QSocketNotifier::Write, &q);
    writing->setEnabled(false);
    QObject::connect(reading.get(), &QSocketNotifier::activated, &q,
                     [this] { dispatch(); });
    QObject::connect(writing.get(), &QSocketNotifier::activated, &q,
                     [this] { flush(); });
    deadline.start();
    flush();
  }
  WaylandIdleObservation &q;
  std::function<int()> opener;
  std::function<bool()> lineageLive;
  QTimer deadline;
  std::unique_ptr<QSocketNotifier> reading, writing;
  wl_display *display = nullptr;
  wl_registry *registry = nullptr;
  wl_callback *sync = nullptr;
  wl_seat *seat = nullptr;
  ext_idle_notifier_v1 *manager = nullptr;
  ext_idle_notification_v1 *notification = nullptr;
  std::vector<std::unique_ptr<NotificationContext>> contexts;
  uint32_t seatName = 0, managerName = 0;
  int seats = 0, notifiers = 0, timeout = 0;
  bool ready = false, isIdle = false, dispatching = false, invalidated = true;
  quint64 generation = 0, notificationGeneration = 0;
};
WaylandIdleObservation::WaylandIdleObservation(
    std::function<int()> opener, std::function<bool()> lineageLive,
    QObject *parent)
    : IdleObservation(parent),
      d(std::make_unique<Private>(*this, std::move(opener),
                                  std::move(lineageLive))) {}
WaylandIdleObservation::~WaylandIdleObservation() = default;
void WaylandIdleObservation::setTimeout(int milliseconds) {
  if (milliseconds < 0 || milliseconds > 1440 * 60000)
    milliseconds = 0;
  if (d->timeout == milliseconds)
    return;
  d->timeout = milliseconds;
  if (!milliseconds)
    d->clear();
  else if (d->display)
    d->arm();
  else
    d->open();
}
bool WaylandIdleObservation::available() const {
  return d->ready && !d->invalidated && d->admitted();
}
bool WaylandIdleObservation::idle() const { return available() && d->isIdle; }
void WaylandIdleObservation::refresh() { d->open(); }
void WaylandIdleObservation::revoke() { d->clear(); }
} // namespace QindaQt::Platform::Idle
