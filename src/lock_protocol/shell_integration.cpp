// SPDX-License-Identifier: GPL-3.0-or-later
#include "configure_policy.h"
#include "protocol_client.h"
#include "ext-session-lock-v1-client-protocol.h"
#include <QtWaylandClient/private/qwaylanddisplay_p.h>
#include <QtWaylandClient/private/qwaylandscreen_p.h>
#include <QtWaylandClient/private/qwaylandshellintegration_p.h>
#include <QtWaylandClient/private/qwaylandshellintegrationplugin_p.h>
#include <QtWaylandClient/private/qwaylandshellsurface_p.h>
#include <QtWaylandClient/private/qwaylandwindow_p.h>
#include <QScreen>
#include <QtGui/qscreen_platform.h>
#include <QHash>
#include <QLoggingCategory>
#include <QPointer>
#include <QWindow>
#include <memory>
using namespace QtWaylandClient;
using namespace QindaQt::LockProtocol;
Q_LOGGING_CATEGORY(lcLockProtocol, "qindaqt.lock.protocol")
namespace {
class LockSurface final : public QWaylandShellSurface {
public:
  LockSurface(QWaylandWindow *window, ext_session_lock_surface_v1 *role)
      : QWaylandShellSurface(window), m_role(role) {
    static const ext_session_lock_surface_v1_listener listener{configure};
    ext_session_lock_surface_v1_add_listener(m_role, &listener, this);
  }
  ~LockSurface() override { ext_session_lock_surface_v1_destroy(m_role); }
  bool isExposed() const override { return m_configured; }
  // Qt normally commits shell construction immediately. The lock protocol
  // requires its first configure to be acknowledged before ANY commit.
  bool commitSurfaceRole() const override { return m_configured; }
  void applyConfigure() override {
    if (!m_pending) return;
    resizeFromApplyConfigure(m_size);
    ext_session_lock_surface_v1_ack_configure(m_role, m_serial);
    m_pending = false; m_configured = true;
    handleActivationChanged(true);
    window()->updateExposure();
  }
private:
  static void configure(void *data, ext_session_lock_surface_v1 *, unsigned serial,
                        unsigned width, unsigned height) {
    auto *self = static_cast<LockSurface *>(data);
    // AGENT-GUARD: never abort on a configure. Skip an unusable one and wait for
    // the next; see configure_policy.h and ADR-0382.
    const auto size = usableConfigureSize(width, height);
    if (!size) {
      qCWarning(lcLockProtocol, "ignoring unusable lock surface configure %ux%u (serial %u); waiting for the next one",
                width, height, serial);
      return;
    }
    self->m_serial = serial; self->m_pending = true; self->m_size = *size;
    self->applyConfigureWhenPossible();
  }
  ext_session_lock_surface_v1 *m_role;
  unsigned m_serial = 0;
  QSize m_size;
  bool m_configured = false, m_pending = false;
};
class LockIntegration final : public QObject, public QWaylandShellIntegration {
public:
  bool initialize(QWaylandDisplay *display) override {
    for (const auto &global : display->globals()) {
      if (global.interface == QStringLiteral("ext_session_lock_manager_v1")) {
        m_client = std::make_unique<ProtocolClient>(display->wl_display(), global.registry, global.id);
        for (const auto &seat : display->globals()) {
          if (seat.interface == QStringLiteral("wl_seat")) { m_client->observeKeyboard(seat.registry, seat.id, seat.version); break; }
        }
        return m_client->available();
      }
    }
    return false;
  }
  QWaylandShellSurface *createShellSurface(QWaylandWindow *window) override {
    if (auto prepared = m_prepared.take(window)) return prepared;
    return prepare(window);
  }
  void *nativeResourceForWindow(const QByteArray &resource, QWindow *window) override {
    if (!window || !window->handle()) return nullptr;
    // Observation only: never assigns a role, creates no protocol object.
    if (resource == nativeObserverResource) return m_client.get();
    if (resource != nativeResource) return nullptr;
    auto *wayland = static_cast<QWaylandWindow *>(window->handle());
    if (dynamic_cast<LockSurface *>(wayland->shellSurface())) return m_client.get();
    // Qt normally assigns a shell role only on show. Preassign the standard
    // immutable role here, before exposure/buffers, then hand it to Qt on show.
    // This makes the native greeter's pre-show role check an actual boundary.
    if (!m_prepared.value(wayland)) {
      auto *surface = prepare(wayland);
      if (!surface) return nullptr;
      m_prepared.insert(wayland, surface);
      // AGENT-GUARD: a role handed out here but never consumed by createShellSurface
      // (the window died before show, e.g. its QML failed to load) belongs to us:
      // Qt only deletes a shell surface it was given. Leaving it would keep the
      // output marked "already has a lock surface" forever and make every retry
      // for that output fail (ADR-0382). Taking it out of m_prepared first keeps a
      // consumed surface (Qt's) from being deleted twice.
      connect(wayland, &QObject::destroyed, this, [this, wayland] { delete m_prepared.take(wayland).data(); });
    }
    return m_client.get();
  }
private:
  LockSurface *prepare(QWaylandWindow *window) {
    // Every refusal says why: the greeter retries the output and the journal is
    // the only record of what the compositor-side lock looked like.
    if (!m_client || !window || !window->window()->screen()) {
      qCWarning(lcLockProtocol, "no lock role: no protocol client or the window has no screen");
      return nullptr;
    }
    auto *screen = window->window()->screen()->nativeInterface<QNativeInterface::QWaylandScreen>();
    if (!screen || !screen->output()) {
      qCWarning(lcLockProtocol, "no lock role: the screen has no wl_output");
      return nullptr;
    }
    if (m_outputs.value(screen->output())) {
      qCWarning(lcLockProtocol, "no lock role: this output already has a lock surface");
      return nullptr;
    }
    auto *role = m_client->createSurface(wlSurfaceForWindow(window), screen->output());
    if (!role) {
      qCWarning(lcLockProtocol, "no lock role: the session lock is no longer available (finished or unlocking)");
      return nullptr;
    }
    auto *surface = new LockSurface(window, role);
    m_outputs.insert(screen->output(), surface);
    return surface;
  }
  QHash<QWaylandWindow *, QPointer<LockSurface>> m_prepared;
  QHash<wl_output *, QPointer<LockSurface>> m_outputs;
  std::unique_ptr<ProtocolClient> m_client;
};
}
class LockIntegrationPlugin final : public QWaylandShellIntegrationPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID QWaylandShellIntegrationFactoryInterface_iid FILE "session-lock.json")
public:
  QWaylandShellIntegration *create(const QString &key, const QStringList &) override {
    return key == QStringLiteral("qindaqt-session-lock") ? new LockIntegration : nullptr;
  }
};
#include "shell_integration.moc"
