// SPDX-License-Identifier: GPL-3.0-or-later
#include "prompt_controller.h"
#include <qindaqt/services/keyring_protocol/prompt_metadata.h>
#include <qindaqt/services/keyring_protocol/wire_types.h>
#include <poll.h>
#include <unistd.h>
#include <cstring>
#include <qindaqt/authentication_overlay/overlay_surface.h>
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <signal.h>

using QindaQt::AuthenticationOverlay::OverlaySurface;
using qindaqt::keyring::service::PromptController;
namespace {
qindaqt::keyring::protocol::PromptMetadata metadataFromPipe() {
    QByteArray frame(1289,Qt::Uninitialized);
    qsizetype used=0;
    QElapsedTimer deadline; deadline.start();
    while(used<frame.size()) {
        pollfd input{STDIN_FILENO,POLLIN,0};
        const auto remaining=2000-deadline.elapsed();
        if(remaining<=0 || poll(&input,1,static_cast<int>(remaining))<=0) throw std::runtime_error("Missing prompt metadata");
        const auto count=read(STDIN_FILENO,frame.data()+used,static_cast<std::size_t>(frame.size()-used));
        if(count<0) throw std::runtime_error("Invalid prompt metadata");
        if(count==0) break;
        used+=count;
    }
    frame.resize(used);
    return qindaqt::keyring::protocol::decodePromptMetadata(frame);
}
}
int main(int argc,char **argv) {
    struct rlimit core{0,0};
    if (setrlimit(RLIMIT_CORE,&core) != 0 || prctl(PR_SET_DUMPABLE,0) != 0) return 2;
    signal(SIGPIPE,SIG_IGN);
    if (!OverlaySurface::initializePlatform()) return 2;
    QGuiApplication app(argc,argv);
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext &,const QString &) {});
    QCommandLineParser parser;
    parser.addOptions({{"action","Prompt action.","action"},{"collection","Collection identifier.","id"}});
    parser.process(app);
    if (parser.value("action") != "create" && parser.value("action") != "unlock"
        && parser.value("action") != "reveal" && parser.value("action") != "change-password" && parser.value("action") != "confirm-delete") return 2;
    PromptController controller; controller.creation = parser.value("action") == "create";
    controller.changing = parser.value("action") == "change-password";
    controller.revealing = parser.value("action") == "reveal";
    controller.confirming = parser.value("action") == "confirm-delete";
    try { controller.label = metadataFromPipe().label; }
    catch (const std::exception &) { return 2; }
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
