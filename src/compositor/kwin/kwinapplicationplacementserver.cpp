// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinapplicationplacementserver.h"
#include "managedwindowregistry.h"
#include "qindaqt-window-management-v1-server-protocol.h"
#include <wayland/display.h>
#include <wayland/clientconnection.h>
#include <wayland/surface.h>
#include <wayland_server.h>
#include <window.h>
#include <workspace.h>
#include <wayland-server-core.h>
#include <QScopedValueRollback>
#include <utility>
namespace QindaQt::Compositor::KWinIntegration {
using ApplicationWindowManagement::Placement;
using ApplicationWindowManagement::Status;
KWinApplicationPlacementServer::KWinApplicationPlacementServer(ManagedWindowRegistry &registry,Apply apply,QObject *parent)
    : QObject(parent),m_registry(registry),m_apply(std::move(apply)),m_timer(this)
{
    if(auto *server=KWin::waylandServer())
        m_global=wl_global_create(*server->display(),&qindaqt_window_manager_v1_interface,1,this,&bind);
    m_timer.setInterval(100);
    connect(&m_timer,&QTimer::timeout,this,&KWinApplicationPlacementServer::process);
    connect(&m_registry,&ManagedWindowRegistry::managedWindowAdded,this,[this] { process(); });
    connect(&m_registry,&ManagedWindowRegistry::managedWindowClosed,this,[this] { process(); });
    m_timer.start();
}
KWinApplicationPlacementServer::~KWinApplicationPlacementServer() {
    m_timer.stop();
    if(m_global) wl_global_destroy(m_global);
    while(!m_managers.isEmpty()) wl_resource_destroy(*m_managers.cbegin());
}
void KWinApplicationPlacementServer::bind(wl_client *client,void *data,uint32_t version,uint32_t objectId) {
    auto *bindingServer=static_cast<KWinApplicationPlacementServer *>(data);
    int owned=0;
    for(auto *resource:bindingServer->m_managers) if(wl_resource_get_client(resource)==client) ++owned;
    auto *managerResource=wl_resource_create(client,&qindaqt_window_manager_v1_interface,static_cast<int>(qMin(version,uint32_t(1))),objectId);
    if(!managerResource) { wl_client_post_no_memory(client); return; }
    if(owned>=4 || bindingServer->m_managers.size()>=256) { wl_resource_post_error(managerResource,0,"too many placement objects"); wl_resource_destroy(managerResource); return; }
    static const struct qindaqt_window_manager_v1_interface implementation={
        [](wl_client *,wl_resource *resource) { wl_resource_destroy(resource); },
        [](wl_client *,wl_resource *resource,uint32_t id,wl_resource *source,wl_resource *created,uint32_t placement) {
            static_cast<KWinApplicationPlacementServer *>(wl_resource_get_user_data(resource))->place(resource,id,source,created,placement);
        },
        [](wl_client *,wl_resource *resource,uint32_t id) {
            auto *self=static_cast<KWinApplicationPlacementServer *>(wl_resource_get_user_data(resource));
            if(self->m_pending.value(resource).contains(id)) self->finish(resource,id,{Status::Cancelled,{},QStringLiteral("placement cancelled")});
        }
    };
    if(auto *connection=KWin::ClientConnection::get(client); connection && !bindingServer->m_rates.contains(connection)) {
        bindingServer->m_rates.insert(connection,{});
        QObject::connect(connection,&KWin::ClientConnection::aboutToBeDestroyed,bindingServer,[bindingServer,connection] { bindingServer->m_rates.remove(connection); });
    }
    bindingServer->m_managers.insert(managerResource);
    wl_resource_set_implementation(managerResource,&implementation,bindingServer,&destroyed);
}
void KWinApplicationPlacementServer::destroyed(wl_resource *resource) {
    auto *self=static_cast<KWinApplicationPlacementServer *>(wl_resource_get_user_data(resource));
    self->m_pending.remove(resource); self->m_managers.remove(resource);
}
void KWinApplicationPlacementServer::finish(wl_resource *manager,quint32 id,ApplicationPlacementResult result) {
    m_pending[manager].remove(id);
    const QByteArray container=result.containerId.toUtf8(),message=result.message.left(256).toUtf8();
    qindaqt_window_manager_v1_send_result(manager,id,static_cast<uint32_t>(result.status),container.constData(),message.constData());
}
void KWinApplicationPlacementServer::place(wl_resource *manager,quint32 id,wl_resource *sourceResource,wl_resource *createdResource,quint32 placement) {
    if(!id || m_pending.value(manager).contains(id)) {
        // A duplicate ID cannot receive two results that the caller confuses.
        wl_resource_post_error(manager,0,"placement request ID must be nonzero and unique while pending"); return;
    }
    auto *connection=KWin::ClientConnection::get(wl_resource_get_client(manager));
    if(!connection || !m_rates.contains(connection)) {
        finish(manager,id,{Status::Denied,{},QStringLiteral("application connection disappeared")}); return;
    }
    auto &rate=m_rates[connection];
    if(rate.reset.hasExpired()) rate=Rate{};
    if(rate.requests>=64) {
        finish(manager,id,{Status::ResourceLimit,{},QStringLiteral("placement request rate exceeded")}); return;
    }
    ++rate.requests;
    auto *source=KWin::SurfaceInterface::get(sourceResource);
    auto *created=KWin::SurfaceInterface::get(createdResource);
    auto *client=wl_resource_get_client(manager);
    if(!source || !created || source==created || placement>static_cast<quint32>(Placement::TileUp)
        || wl_resource_get_client(sourceResource)!=client || wl_resource_get_client(createdResource)!=client) {
        finish(manager,id,{Status::Invalid,{},QStringLiteral("surfaces must be distinct and owned by this connection")}); return;
    }
    auto *server=KWin::waylandServer();
    auto *sourceWindow=server ? server->findWindow(source) : nullptr;
    if(!server || server->isScreenLocked() || !sourceWindow || m_registry.windowId(sourceWindow).isEmpty()) {
        finish(manager,id,{Status::Denied,{},QStringLiteral("source is unavailable or session is locked")}); return;
    }
    auto *createdWindow=server->findWindow(created);
    auto *active=KWin::workspace()->activeWindow();
    if(active!=sourceWindow && active!=createdWindow) {
        finish(manager,id,{Status::Denied,{},QStringLiteral("application is not foreground")}); return;
    }
    if(m_pending.value(manager).size()>=8) {
        finish(manager,id,{Status::ResourceLimit,{},QStringLiteral("too many pending placements")}); return;
    }
    m_pending[manager].insert(id,{source,created,static_cast<Placement>(placement),QDeadlineTimer(5'000)});
    process();
}
void KWinApplicationPlacementServer::process() {
    if(m_processing) return;
    QScopedValueRollback processing(m_processing,true);
    const auto managers=m_pending.keys();
    for(auto *manager:managers) {
        const auto ids=m_pending.value(manager).keys();
        for(auto id:ids) {
            const Pending pending=m_pending.value(manager).value(id);
            auto *server=KWin::waylandServer();
            if(!pending.source || !pending.created || !server || server->isScreenLocked()) {
                finish(manager,id,{Status::Denied,{},QStringLiteral("window disappeared or session is locked")}); continue;
            }
            auto *source=server->findWindow(pending.source);
            auto *created=server->findWindow(pending.created);
            const QString sourceId=m_registry.windowId(source),createdId=m_registry.windowId(created);
            if(sourceId.isEmpty()) { finish(manager,id,{Status::Denied,{},QStringLiteral("source disappeared")}); continue; }
            if(createdId.isEmpty()) {
                if(pending.deadline.hasExpired()) finish(manager,id,{Status::Timeout,{},QStringLiteral("new window did not map before deadline")});
                continue;
            }
            // Remove before synchronous scene publication: registry invalidation
            // can call process(), but may never execute this request twice.
            m_pending[manager].remove(id);
            finish(manager,id,m_apply(sourceId,createdId,pending.placement));
        }
    }
}
}
