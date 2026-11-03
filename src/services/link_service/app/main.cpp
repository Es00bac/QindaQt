// SPDX-License-Identifier: GPL-3.0-or-later
// qindaqt-link-service: owns the QindaLink identity, trust store, loopback
// TLS listener and ADB tunnels, and exports org.qindaqt.Link1.
#include <qindaqt/services/link_adb/adb_runner.h>
#include <qindaqt/services/link_notifications/fdo_phone_presenter.h>
#include <qindaqt/services/link_notifications/process_reply_launcher.h>
#include <qindaqt/services/link_notifications/recording_presenter.h>
#include <qindaqt/services/link_service/caller_policy.h>
#include <qindaqt/services/link_service/link1_service.h>
#include <qindaqt/services/link_service/qt_notification_presenter.h>

#include "input_composition.h"
#include "native_input_gate.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>
#include <QtDBus/QDBusConnection>

#include <QtCore/QSocketNotifier>

#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>

#include <memory>

using namespace QindaQt::Link;

namespace {
int g_signalPipe[2] = {-1, -1};
// Async-signal-safe: only writes one byte; the real shutdown runs on the event loop.
void onTerminate(int) { const char c = 1; const ssize_t r = ::write(g_signalPipe[1], &c, 1); (void)r; }
}

int main(int argc, char **argv)
{
    // AGENT-GUARD: the service may hold secret-typing traffic in memory later
    // (architecture page, "Secret typing"); no core dumps or ptrace attach.
    prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-link-service"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    // AGENT-NOTE: the options exist so tests run against a private bus,
    // a temporary data directory and an ephemeral port without touching the
    // user's real state. Defaults are the production values.
    QCommandLineOption dataDir(QStringLiteral("data-dir"), QStringLiteral("State directory."), QStringLiteral("dir"));
    QCommandLineOption port(QStringLiteral("port"), QStringLiteral("Loopback port (0 = ephemeral)."),
                            QStringLiteral("port"), QStringLiteral("47160"));
    QCommandLineOption noAdb(QStringLiteral("no-adb"), QStringLiteral("Do not track or tunnel ADB devices."));
    QCommandLineOption noNotify(QStringLiteral("no-notifications"),
                                QStringLiteral("Do not post pairing notifications."));
    QCommandLineOption recordingNotifier(QStringLiteral("recording-notifier"),
                                         QStringLiteral("Test only: record phone notifications to FILE instead of "
                                                        "posting them to the notification server."),
                                         QStringLiteral("file"));
    QCommandLineOption configDir(QStringLiteral("config-dir"), QStringLiteral("Settings directory."), QStringLiteral("dir"));
    QCommandLineOption cacheDir(QStringLiteral("cache-dir"), QStringLiteral("Icon cache directory."), QStringLiteral("dir"));
    
    QCommandLineOption noInput(QStringLiteral("no-input"),
                               QStringLiteral("Do not offer the input and macro capabilities."));
    QCommandLineOption stateDir(QStringLiteral("state-dir"), QStringLiteral("State directory for the portal restore token."),
                                QStringLiteral("dir"));
    QCommandLineOption trustedCaller(QStringLiteral("trusted-caller"),
                                     QStringLiteral("TEST/DEVELOPMENT ONLY: extra executable allowed to use protected Link1 members (repeatable). Never use in an installed session."),
                                     QStringLiteral("exe"));
    parser.addOptions({dataDir, port, noAdb, noNotify, recordingNotifier, configDir, cacheDir, noInput, stateDir, trustedCaller});
    parser.process(app);

    ServiceConfig config;
    config.dataDir = parser.isSet(dataDir)
        ? parser.value(dataDir)
        : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/qindalink");
    config.port = quint16(parser.value(port).toUInt());
    config.version = QStringLiteral(QINDAQT_VERSION);

    config.configDir = parser.isSet(configDir)
        ? parser.value(configDir)
        : QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/qindalink");
    config.cacheDir = parser.isSet(cacheDir)
        ? parser.value(cacheDir)
        : QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/qindalink");
    // SIGTERM/SIGINT end the event loop so input state is released in an orderly way
    // (held keys up, portal session closed) instead of dying with a key held.
    std::unique_ptr<QSocketNotifier> signalNotifier;
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, g_signalPipe) == 0) {
        signalNotifier = std::make_unique<QSocketNotifier>(g_signalPipe[0], QSocketNotifier::Read);
        QObject::connect(signalNotifier.get(), &QSocketNotifier::activated, &app, [&app] { app.quit(); });
        struct sigaction sa{};
        sa.sa_handler = onTerminate;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGINT, &sa, nullptr);
    }

    QDBusConnection bus = QDBusConnection::sessionBus();
    // Exit when the constructing bus goes away; reconnecting would resume with stale state.
    bus.connect(QString{}, QStringLiteral("/org/freedesktop/DBus/Local"), QStringLiteral("org.freedesktop.DBus.Local"),
                QStringLiteral("Disconnected"), &app, SLOT(quit()));

    std::unique_ptr<PairingPresenter> presenter;
    if (!parser.isSet(noNotify)) {
        presenter = std::make_unique<QtNotificationPresenter>(bus);
    }
    // AGENT-GUARD: phone notification content goes only to the injected presenter. Tests pass
    // --recording-notifier; --no-notifications alone leaves the `notify` capability off.
    std::unique_ptr<NotificationPresenter> phonePresenter;
    ProcessReplyLauncher replyLauncher;
    if (parser.isSet(recordingNotifier)) {
        phonePresenter = std::make_unique<RecordingPresenter>(parser.value(recordingNotifier));
    } else if (!parser.isSet(noNotify)) {
        phonePresenter = std::make_unique<FdoPhoneNotificationPresenter>(bus);
    }
    config.phoneNotifications = phonePresenter.get();
    config.replyLauncher = &replyLauncher;
    std::unique_ptr<AdbRunner> adb;
    if (!parser.isSet(noAdb)) {
        adb = std::make_unique<QProcessAdbRunner>();
    }
    // AGENT-GUARD (ADR-0438): the trusted set is fixed here, from the installed QindaLink clients plus
    // --trusted-caller (tests). Never read it from the environment or a D-Bus call.
    QStringList trusted{QStringLiteral(QINDAQT_BINDIR "/qindaqt-link"), QStringLiteral(QINDAQT_BINDIR "/qindaqt-shell")};
    for (const QString &extra : parser.values(trustedCaller)) {
        // Stderr is the journal under the systemd user unit.
        qWarning("qindaqt-link-service: WARNING --trusted-caller %s is a test/development override; it widens who may read messages and act as the user", qPrintable(extra));
        trusted += extra;
    }
    CallerPolicy callerPolicy(bus, std::make_unique<DBusProcessInspector>(bus), trusted);
    config.callerAuthority = &callerPolicy;
    Link1Service service(config, bus, presenter.get(), adb.get());
    QString error;
    if (!service.start(&error)) {
        qCritical("qindaqt-link-service: %s", qPrintable(error));
        return 1;
    }

    // Input and macros (ADR-0436). Composed after start() and before the event
    // loop, so no connection can arrive before the features are registered.
    // Nothing contacts the RemoteDesktop portal until a phone sends input.begin.
    std::unique_ptr<NativeInputGate> gate;
    std::unique_ptr<InputComposition> input;
    if (!parser.isSet(noInput)) {
        gate = std::make_unique<NativeInputGate>(bus, qEnvironmentVariable("XDG_RUNTIME_DIR"),
                                                 qEnvironmentVariable("WAYLAND_DISPLAY"));
        InputCompositionOptions options;
        const QString state = parser.isSet(stateDir)
            ? parser.value(stateDir)
            : QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/qindalink");
        options.portal.tokenFile = state + QStringLiteral("/restore-token");
        options.gate = gate.get();
        input = std::make_unique<InputComposition>(*service.hub(), bus, std::move(options));
    }
    const int rc = app.exec();
    // AGENT-GUARD: release before the composition is destroyed so the portal session is still open for the key-ups.
    if (input) input->shutdown();
    return rc;
}
