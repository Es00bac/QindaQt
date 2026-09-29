// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/application_window_management/client.h>
#include "qindaqt-window-management-v1-client-protocol.h"
#include <QtWaylandClient/QWaylandClientExtension>
#include <KWayland/Client/surface.h>
#include <QGuiApplication>
#include <QHash>
#include <QVector>
#include <QPointer>
#include <QThread>
#include <QTimer>
#include <QWindow>
#include <wayland-client.h>

namespace QindaQt::ApplicationWindowManagement {
class WindowPlacementClient::Impl final : public QWaylandClientExtension {
public:
    explicit Impl(WindowPlacementClient &clientOwner) : QWaylandClientExtension(1), owner(clientOwner) {
        QObject::connect(this, &QWaylandClientExtension::activeChanged, &owner, [this] {
            Q_EMIT this->owner.availableChanged(isActive() && manager);
            if (!isActive()) {
                const auto ids=pending.keys();
                for (auto id : ids) complete(id, Status::Unavailable, {}, QStringLiteral("window manager disconnected"));
            }
        });
        if (QGuiApplication::platformName()==QStringLiteral("wayland")) initialize();
    }
    ~Impl() override { if (manager) qindaqt_window_manager_v1_destroy(manager); }
    const wl_interface *extensionInterface() const override { return &qindaqt_window_manager_v1_interface; }
    void bind(wl_registry *registry, int name, int version) override {
        setVersion(qMin(version, 1));
        manager=static_cast<qindaqt_window_manager_v1 *>(wl_registry_bind(registry, static_cast<uint32_t>(name), extensionInterface(), 1));
        static const qindaqt_window_manager_v1_listener listener={[](void *data, qindaqt_window_manager_v1 *, uint32_t id, uint32_t status, const char *container, const char *message) {
            auto *self=static_cast<Impl *>(data);
            self->complete(id, status<=static_cast<uint32_t>(Status::ResourceLimit) ? static_cast<Status>(status) : Status::Invalid,
                           QString::fromUtf8(container), QString::fromUtf8(message));
        }};
        qindaqt_window_manager_v1_add_listener(manager, &listener, this);
    }
    void complete(quint32 id, Status status, QString container, QString message) {
        const auto found=pending.find(id);
        if(found==pending.end()) return;
        for(const auto &connection:found->connections) QObject::disconnect(connection);
        pending.erase(found);
        Q_EMIT owner.finished(id,status,container,message);
    }
    WindowPlacementClient &owner;
    qindaqt_window_manager_v1 *manager=nullptr;
    struct Pending final {
        QPointer<QWindow> source,created;
        QVector<QMetaObject::Connection> connections;
    };
    QHash<quint32,Pending> pending;
    quint32 serial=0;
};
WindowPlacementClient::WindowPlacementClient(QObject *parent) : QObject(parent) {
    // AGENT-GUARD: constructing a Wayland extension on another QPA backend warns
    // before initialize(); absence must remain a quiet ordinary-window fallback.
    if (QGuiApplication::platformName() == QStringLiteral("wayland"))
        m_impl = std::make_unique<Impl>(*this);
}
WindowPlacementClient::~WindowPlacementClient()=default;
bool WindowPlacementClient::available() const noexcept { return m_impl && m_impl->isActive() && m_impl->manager; }
quint32 WindowPlacementClient::place(QWindow *source, QWindow *created, Placement placement, QString *error) {
    const auto refuse=[error](const QString &message) { if(error) *error=message; return quint32(0); };
    if (QThread::currentThread()!=thread()) return refuse(QStringLiteral("window placement must run on the GUI thread"));
    if (!available()) return refuse(QStringLiteral("container placement is unavailable"));
    if (!source || !created || source==created || static_cast<quint32>(placement)>static_cast<quint32>(Placement::TileUp))
        return refuse(QStringLiteral("invalid placement request"));
    if (m_impl->pending.size()>=8) return refuse(QStringLiteral("too many pending placements"));
    auto *sourceSurface=KWayland::Client::Surface::fromWindow(source);
    auto *createdSurface=KWayland::Client::Surface::fromWindow(created);
    if (!sourceSurface || !createdSurface) return refuse(QStringLiteral("both windows need native Wayland surfaces"));
    do { ++m_impl->serial; } while(!m_impl->serial || m_impl->pending.contains(m_impl->serial));
    const quint32 id=m_impl->serial;
    m_impl->pending.insert(id, {source,created,{}});
    qindaqt_window_manager_v1_place(m_impl->manager,id,*sourceSurface,*createdSurface,static_cast<quint32>(placement));
    const auto gone=[this,id] { cancel(id); };
    auto &pending=m_impl->pending[id];
    pending.connections.append(QObject::connect(source,&QObject::destroyed,this,gone));
    pending.connections.append(QObject::connect(created,&QObject::destroyed,this,gone));
    QTimer::singleShot(6'000,this,[this,id] {
        if(!m_impl->pending.contains(id)) return;
        cancel(id);
        m_impl->complete(id,Status::Timeout,{},QStringLiteral("window manager did not finish placement"));
    });
    return id;
}
void WindowPlacementClient::cancel(quint32 requestId) {
    if(QThread::currentThread()!=thread() || !available() || !m_impl->pending.contains(requestId)) return;
    qindaqt_window_manager_v1_cancel(m_impl->manager,requestId);
}
}
