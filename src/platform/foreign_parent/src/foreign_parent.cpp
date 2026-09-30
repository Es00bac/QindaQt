// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/platform/foreign_parent/foreign_parent.h>
#include <QGuiApplication>
#include <QtGui/qguiapplication_platform.h>
#include <wayland-client.h>
#include <functional>
#include <QPointer>
#include <QTimer>
#include <QWindow>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtWaylandClient/QWaylandClientExtension>
#include "qwayland-xdg-foreign-unstable-v2.h"
namespace QindaQt::Platform::ForeignParent {
namespace {
class Importer final : public QWaylandClientExtensionTemplate<Importer>,
                       public QtWayland::zxdg_importer_v2 {
public:
    Importer() : QWaylandClientExtensionTemplate<Importer>(1) { initialize(); }
    ~Importer() override { if (isInitialized()) destroy(); }
};
class Imported final : public QtWayland::zxdg_imported_v2 {
public:
    Imported(::zxdg_imported_v2 *object, std::function<void()> gone)
        : QtWayland::zxdg_imported_v2(object), m_gone(std::move(gone)) {}
    ~Imported() override { if (isInitialized()) destroy(); }
protected:
    void zxdg_imported_v2_destroyed() override { m_gone(); }
private:
    std::function<void()> m_gone;
};
}
class ForeignParent::Private {
public:
    ForeignParent &q;
    Importer importer;
    std::unique_ptr<Imported> imported;
    QPointer<QWindow> window;
    QTimer deadline;
    ::wl_callback *barrier = nullptr;
    QString handle;
    quint64 generation = 0;
    bool attached = false;
    explicit Private(ForeignParent &object) : q(object) {
        deadline.setSingleShot(true);
        QObject::connect(&deadline, &QTimer::timeout, &q, [this] { fail(); });
        QObject::connect(&importer, &QWaylandClientExtension::activeChanged, &q, [this] {
            if (!attached) return;
            if (!importer.isActive()) fail(); else bind();
        });
    }
    ~Private() { if (barrier) wl_callback_destroy(barrier); }
    void fail() {
        if (!attached) return;
        attached = false; ++generation; deadline.stop();
        if (barrier) { wl_callback_destroy(barrier); barrier = nullptr; }
        // Invalidation precedes deferred teardown: protocol listeners may be
        // dispatching the proxy that reported a destroyed foreign parent.
        const auto retired = generation;
        QTimer::singleShot(0, &q, [this, retired] { if (retired == generation) imported.reset(); });
        Q_EMIT q.lost(); // Last action: consumers may destroy/rebind the adapter.
    }
    void bind() {
        if (!attached || imported || !window || !importer.isActive()) return;
        auto *native = QGuiApplication::platformNativeInterface();
        auto *surface = native ? static_cast<::wl_surface *>(native->nativeResourceForWindow(
            QByteArrayLiteral("surface"), window)) : nullptr;
        if (!surface) { fail(); return; }
        const auto current = generation;
        imported = std::make_unique<Imported>(importer.import_toplevel(handle), [this, current] {
            if (generation == current) fail();
        });
        imported->set_parent_of(surface);
        auto *application = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (!application) { fail(); return; }
        barrier = wl_display_sync(application->display());
        static const wl_callback_listener listener{[](void *data, wl_callback *callback, uint32_t) {
            auto *self = static_cast<Private *>(data);
            if (self->barrier != callback) return;
            wl_callback_destroy(callback); self->barrier = nullptr;
            if (self->attached) { self->deadline.stop(); Q_EMIT self->q.ready(); }
        }};
        wl_callback_add_listener(barrier, &listener, this);
        // xdg-foreign has no positive import event. A display barrier admits
        // presentation only after the server processed import/set_parent_of;
        // an invalid handle's destroyed event is dispatched before readiness.
        wl_display_flush(application->display());
    }
};
ForeignParent::ForeignParent(QObject *parent) : QObject(parent), d(std::make_unique<Private>(*this)) {}
ForeignParent::~ForeignParent() = default;
void ForeignParent::clear() {
    ++d->generation; d->attached = false; d->deadline.stop();
    if (d->barrier) { wl_callback_destroy(d->barrier); d->barrier = nullptr; }
    d->imported.reset(); d->window = nullptr;
}
void ForeignParent::attach(QWindow &window, const QString &handle) {
    clear(); d->window = &window;
    if (handle.isEmpty()) {
        const auto generation = d->generation;
        QTimer::singleShot(0, this, [this, generation] { if (generation == d->generation) Q_EMIT ready(); }); return;
    }
    if (!handle.startsWith(QStringLiteral("wayland:")) || handle.size() <= 8 || handle.size() > 512) {
        const auto generation = d->generation;
        QTimer::singleShot(0, this, [this, generation] { if (generation == d->generation) Q_EMIT lost(); }); return;
    }
    d->handle = handle.mid(8); d->attached = true;
    const auto generation = d->generation;
    connect(&window, &QObject::destroyed, this, [this, generation] { if (generation == d->generation) d->fail(); });
    d->deadline.start(2000); d->bind();
}
} // namespace QindaQt::Platform::ForeignParent
