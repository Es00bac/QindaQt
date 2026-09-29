// SPDX-License-Identifier: GPL-3.0-or-later
#include "../daemon/secret_service.h"
#include "../daemon/process_prompt_provider.h"
#include "../daemon/control_server.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QTimer>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QSocketNotifier>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <unistd.h>

using namespace qindaqt::keyring::service;
int main(int argc,char **argv) {
    struct rlimit core{0,0};
    if (setrlimit(RLIMIT_CORE,&core) != 0 || prctl(PR_SET_DUMPABLE,0) != 0) return 2;
    sigset_t signalMask; sigemptyset(&signalMask); sigaddset(&signalMask,SIGTERM); sigaddset(&signalMask,SIGINT);
    if (sigprocmask(SIG_BLOCK,&signalMask,nullptr) != 0) return 2;
    const int signalFd = signalfd(-1,&signalMask,SFD_CLOEXEC | SFD_NONBLOCK);
    if (signalFd < 0) return 2;
    Q_INIT_RESOURCE(keyring_api);
    QCoreApplication application(argc,argv);
    QCoreApplication::setApplicationName("qindaqt-keyring");
    qInstallMessageHandler([](QtMsgType,const QMessageLogContext &,const QString &) {});
    QCommandLineParser parser; parser.addHelpOption();
    parser.addOptions({{"storage-root","Explicit collection directory.","path"},
                       {"runtime-root","Explicit private runtime directory.","path"},
                       {"private-bus","Explicit isolated bus address for qualification.","address"},
                       {"prompt-program","Explicit trusted prompt helper executable.","path"}});
    parser.process(application);
    const auto directory = parser.isSet("storage-root") ? parser.value("storage-root")
        : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/qindaqt/keyring";
    const auto helper = parser.isSet("prompt-program") ? parser.value("prompt-program")
        : QString::fromUtf8(QINDAQT_KEYRING_PROMPT_PATH);
    auto bus = parser.isSet("private-bus")
        ? QDBusConnection::connectToBus(parser.value("private-bus"),"qindaqt-keyring-private")
        : QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.registerService("org.freedesktop.secrets")) return 3;
    if (!bus.registerService(NativeInterface)) { bus.unregisterService("org.freedesktop.secrets"); return 3; }
    int result = 2;
    try {
        CollectionRepository repository(directory);
        const auto runtime = parser.isSet("runtime-root") ? parser.value("runtime-root")
            : QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/qindaqt-keyring";
        const bool activated = qEnvironmentVariableIntValue("LISTEN_FDS") == 1
            && qEnvironmentVariable("LISTEN_PID").toLongLong() == getpid();
        ControlServer control(runtime,repository,activated ? 3 : -1);
        ProcessPromptProvider provider(helper);
        SecretService service(repository,provider,bus);
        QObject::connect(&control,&ControlServer::collectionStateChanged,&service,&SecretService::notifyCollectionState);
        if (!bus.registerVirtualObject(Root,&service,QDBusConnection::SubPath)) return 2;
        QSocketNotifier ending(signalFd,QSocketNotifier::Read);
        QObject::connect(&ending,&QSocketNotifier::activated,&application,[&] {
            signalfd_siginfo info{}; ::read(signalFd,&info,sizeof(info)); application.quit();
        });
        QTimer authority;
        QObject::connect(&authority,&QTimer::timeout,&application,[&] {
            if (!bus.isConnected()) { application.exit(1); return; }
            const auto owner = bus.interface()->serviceOwner("org.freedesktop.secrets");
            if (!owner.isValid() || owner.value() != bus.baseService()) application.exit(1);
        });
        authority.start(250);
        result = application.exec();
        bus.unregisterObject(Root,QDBusConnection::UnregisterTree);
        // Destruction retires all stored/volatile secret pages before releasing names.
    } catch (const std::exception &) { result = 2; }
    bus.unregisterService(NativeInterface); bus.unregisterService("org.freedesktop.secrets");
    close(signalFd); return result;
}
