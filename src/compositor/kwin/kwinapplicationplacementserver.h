// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QDeadlineTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <functional>
#include <qindaqt/application_window_management/types.h>
struct wl_global;
struct wl_resource;
struct wl_client;
namespace KWin {
class SurfaceInterface;
class ClientConnection;
} // namespace KWin
namespace QindaQt::Compositor::KWinIntegration {
class ManagedWindowRegistry;
struct ApplicationPlacementResult final {
  ApplicationWindowManagement::Status status =
      ApplicationWindowManagement::Status::Unavailable;
  QString containerId;
  QString message;
};
// GUI-thread transport; registry and apply callback outlive this object.
// Surface ownership is authenticated by Wayland resources, never
// PID/app_id/UUID claims. Owns bounded pending requests and cancels them before
// withdrawing the global.
class KWinApplicationPlacementServer final : public QObject {
public:
  using Apply = std::function<ApplicationPlacementResult(
      const QString &, const QString &,
      ApplicationWindowManagement::Placement)>;
  KWinApplicationPlacementServer(ManagedWindowRegistry &registry, Apply apply,
                                 QObject *parent = nullptr);
  ~KWinApplicationPlacementServer() override;

private:
  struct Pending final {
    QPointer<KWin::SurfaceInterface> source;
    QPointer<KWin::SurfaceInterface> created;
    ApplicationWindowManagement::Placement placement;
    QDeadlineTimer deadline{5'000};
  };
  void place(wl_resource *manager, quint32 id, wl_resource *source,
             wl_resource *created, quint32 placement);
  void finish(wl_resource *manager, quint32 id,
              ApplicationPlacementResult result);
  void process();
  static void bind(wl_client *client, void *data, uint32_t version,
                   uint32_t id);
  static void destroyed(wl_resource *resource);
  ManagedWindowRegistry &m_registry;
  Apply m_apply;
  wl_global *m_global = nullptr;
  QSet<wl_resource *> m_managers;
  QHash<wl_resource *, QHash<quint32, Pending>> m_pending;
  struct Rate final {
    QDeadlineTimer reset{10'000};
    unsigned requests = 0;
  };
  QHash<KWin::ClientConnection *, Rate> m_rates;
  QTimer m_timer;
  bool m_processing = false;
};
} // namespace QindaQt::Compositor::KWinIntegration
