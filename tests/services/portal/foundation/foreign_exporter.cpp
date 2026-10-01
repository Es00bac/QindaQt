// SPDX-License-Identifier: GPL-3.0-or-later
#include <QGuiApplication>
#include <QQuickWindow>
#include <QTimer>
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtWaylandClient/QWaylandClientExtension>
#include "qwayland-xdg-foreign-unstable-v2.h"
#include <unistd.h>
#include <memory>
class Exporter final : public QWaylandClientExtensionTemplate<Exporter>, public QtWayland::zxdg_exporter_v2 {
public:
    Exporter() : QWaylandClientExtensionTemplate<Exporter>(1) { initialize(); }
    ~Exporter() override { if (isInitialized()) destroy(); }
};
class Exported final : public QtWayland::zxdg_exported_v2 {
public:
    explicit Exported(::zxdg_exported_v2 *object) : QtWayland::zxdg_exported_v2(object) {}
    ~Exported() override { if (isInitialized()) destroy(); }
protected:
    void zxdg_exported_v2_handle(const QString &handle) override {
        const auto receipt = (QStringLiteral("wayland:") + handle + QLatin1Char('\n')).toUtf8();
        if (write(STDOUT_FILENO, receipt.constData(), static_cast<size_t>(receipt.size())) != receipt.size())
            QCoreApplication::exit(2);
    }
};
int main(int argc, char **argv) {
    if (qEnvironmentVariable("WAYLAND_SOCKET").isEmpty()) return 2;
    QGuiApplication app(argc, argv); QQuickWindow window; window.resize(240, 120); window.show();
    Exporter exporter; std::unique_ptr<Exported> exported;
    QTimer mapped; mapped.setInterval(10);
    QObject::connect(&mapped, &QTimer::timeout, &app, [&] {
        if (!window.isExposed() || !exporter.isActive()) return;
        auto *native = QGuiApplication::platformNativeInterface();
        auto *surface = static_cast<::wl_surface *>(native->nativeResourceForWindow(QByteArrayLiteral("surface"), &window));
        if (!surface) { app.exit(2); return; }
        exported = std::make_unique<Exported>(exporter.export_toplevel(surface)); mapped.stop();
    });
    mapped.start(); QTimer::singleShot(15000, &app, &QCoreApplication::quit); return app.exec();
}
