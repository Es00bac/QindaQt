// SPDX-License-Identifier: GPL-3.0-or-later
// qindaqt-link-service: owns the QindaLink identity, trust store, loopback
// TLS listener and ADB tunnels, and exports org.qindaqt.Link1.
#include <qindaqt/services/link_adb/adb_runner.h>
#include <qindaqt/services/link_service/link1_service.h>
#include <qindaqt/services/link_service/qt_notification_presenter.h>

#include "input_composition.h"
#include "native_input_gate.h"

#include <QtCore/QCommandLineParser>
#include <QtCore/QCoreApplication>
#include <QtCore/QStandardPaths>
#include <QtDBus/QDBusConnection>

#include <sys/prctl.h>

#include <memory>

using namespace QindaQt::Link;

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
    QCommandLineOption noInput(QStringLiteral("no-input"),
                               QStringLiteral("Do not offer the input and macro capabilities."));
    QCommandLineOption stateDir(QStringLiteral("state-dir"), QStringLiteral("State directory for the portal restore token."),
                                QStringLiteral("dir"));
    parser.addOptions({dataDir, port, noAdb, noNotify, noInput, stateDir});
    parser.process(app);

    ServiceConfig config;
    config.dataDir = parser.isSet(dataDir)
        ? parser.value(dataDir)
        : QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/qindalink");
    config.port = quint16(parser.value(port).toUInt());
    config.version = QStringLiteral(QINDAQT_VERSION);

    QDBusConnection bus = QDBusConnection::sessionBus();
    // Exit when the constructing bus goes away; reconnecting would resume with stale state.
    bus.connect(QString{}, QStringLiteral("/org/freedesktop/DBus/Local"), QStringLiteral("org.freedesktop.DBus.Local"),
                QStringLiteral("Disconnected"), &app, SLOT(quit()));

    std::unique_ptr<PairingPresenter> presenter;
    if (!parser.isSet(noNotify)) {
        presenter = std::make_unique<QtNotificationPresenter>(bus);
    }
    std::unique_ptr<AdbRunner> adb;
    if (!parser.isSet(noAdb)) {
        adb = std::make_unique<QProcessAdbRunner>();
    }
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
    return app.exec();
}
