// SPDX-License-Identifier: GPL-3.0-or-later
#include "prompt_controller.h"
#include <qindaqt/authentication_overlay/overlay_surface.h>
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <signal.h>

using QindaQt::AuthenticationOverlay::OverlaySurface;
using qindaqt::keyring::service::PromptController;
int main(int argc,char **argv) {
    struct rlimit core{0,0};
    if (setrlimit(RLIMIT_CORE,&core) != 0 || prctl(PR_SET_DUMPABLE,0) != 0) return 2;
    signal(SIGPIPE,SIG_IGN);
    if (!OverlaySurface::initializePlatform()) return 2;
    QGuiApplication app(argc,argv);
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext &,const QString &) {});
    QCommandLineParser parser;
    parser.addOptions({{"action","Prompt action.","action"},{"collection","Collection identifier.","id"},
                       {"label","Collection label.","label"},{"caller","Authenticated caller.","caller"}});
    parser.process(app);
    if (parser.value("action") != "create" && parser.value("action") != "unlock") return 2;
    PromptController controller; controller.creation = parser.value("action") == "create";
    controller.label = parser.value("label").left(1024);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("prompt",&controller);
    engine.loadFromModule("QindaQt.KeyringPrompt","Main");
    if (engine.rootObjects().isEmpty()) return 2;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window || !OverlaySurface::configure(*window,"qindaqt-keyring-prompt")) return 2;
    window->show();
    QTimer::singleShot(30000,&controller,&PromptController::cancel);
    return app.exec();
}
