// SPDX-License-Identifier: GPL-3.0-or-later
#include "frame_provider.h"
#include "viewer_actions.h"
#include "viewer_controller.h"
#include <qindaqt/app_shell/application_coordinator.h>
#include <qindaqt/app_shell/menu_export/first_party_composition.h>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-viewer"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("QindaQt Viewer"));
    QCoreApplication::setOrganizationName(QStringLiteral("QindaQt"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qindaqt.Viewer"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("QindaQt image and PDF viewer (GPL-3.0-or-later)"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Local image/PDF path or file URL"),
                                 QStringLiteral("[file]"));
    parser.addOption({QStringLiteral("screenshot"),
        QStringLiteral("Save the settled window to PNG and exit (verification)"), QStringLiteral("path")});
    parser.process(application);
    if (parser.positionalArguments().size() > 1) {
        std::fprintf(stderr, "qindaqt-viewer: open one file per window\n");
        return 2;
    }

    QindaQt::Viewer::ViewerController viewer;
    QindaQt::AppShell::ApplicationCoordinator coordinator;
    coordinator.setApplicationName(QStringLiteral("QindaQt Viewer"));
    const auto actions = QindaQt::Viewer::viewerActions();
    if (const auto error = coordinator.replaceActions(actions); !error.ok()) {
        std::fprintf(stderr, "qindaqt-viewer: %s\n", qPrintable(error.message));
        return 1;
    }
    const auto updateActions = [&] {
        for (const auto &action : actions) {
            bool enabled = true;
            if (action.menuId == QStringLiteral("view")) enabled = viewer.ready();
            if (action.id == QStringLiteral("file.close")) enabled = !viewer.fileName().isEmpty();
            if (action.id == QStringLiteral("view.previous") || action.id == QStringLiteral("view.first"))
                enabled = viewer.ready() && viewer.page() > 0;
            if (action.id == QStringLiteral("view.next") || action.id == QStringLiteral("view.last"))
                enabled = viewer.ready() && viewer.page() + 1 < viewer.pageCount();
            static_cast<void>(coordinator.setActionEnabled(action.id, enabled));
        }
    };
    QObject::connect(&viewer, &QindaQt::Viewer::ViewerController::stateChanged, &coordinator, updateActions);
    updateActions();

    QQmlApplicationEngine engine;
    auto *provider = new QindaQt::Viewer::FrameProvider;
    engine.addImageProvider(QStringLiteral("document"), provider);
    QObject::connect(&viewer, &QindaQt::Viewer::ViewerController::frameChanged,
                     &engine, [&] { provider->setFrame(viewer.frame()); });
    engine.rootContext()->setContextProperty(QStringLiteral("viewer"), &viewer);
    engine.rootContext()->setContextProperty(QStringLiteral("coordinator"), &coordinator);
    engine.loadFromModule(QStringLiteral("QindaQt.Viewer"), QStringLiteral("Main"));
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0));
    if (!window) return 1;
    QPointer<QQuickWindow> windowGuard(window);
    auto menuExport = QindaQt::AppShell::MenuExport::composeFirstPartyMenuExport(
        coordinator, *window, QDBusConnection::sessionBus(), [windowGuard](bool visible) {
            if (windowGuard) windowGuard->setProperty("inWindowMenuVisible", visible);
        });
    if (!parser.positionalArguments().isEmpty())
        viewer.open(QindaQt::Viewer::ViewerController::localArgument(parser.positionalArguments().first()));

    QTimer capture;
    if (parser.isSet(QStringLiteral("screenshot"))) {
        capture.setInterval(250);
        QObject::connect(&capture, &QTimer::timeout, &application, [&] {
            if (viewer.busy()) return;
            capture.stop();
            const bool saved = window->grabWindow().save(parser.value(QStringLiteral("screenshot")));
            std::fprintf(stdout, "pages=%d page=%d rendered=%d\n", viewer.pageCount(), viewer.page() + 1,
                         viewer.ready() ? 1 : 0);
            QCoreApplication::exit(saved && viewer.error().isEmpty() ? 0 : 1);
        });
        capture.start();
        QTimer::singleShot(30000, &application, [] { QCoreApplication::exit(3); });
    }
    const int status = application.exec();
    menuExport.reset();
    return status;
}
