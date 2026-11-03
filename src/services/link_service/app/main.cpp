// SPDX-License-Identifier: GPL-3.0-or-later
// qindaqt-link-service: owns the QindaLink identity, trust store, loopback
// TLS listener and ADB tunnels, and exports org.qindaqt.Link1.
#include <qindaqt/services/link_adb/adb_runner.h>
#include <qindaqt/services/link_notifications/fdo_phone_presenter.h>
#include <qindaqt/services/link_notifications/process_reply_launcher.h>
#include <qindaqt/services/link_notifications/recording_presenter.h>
#include <qindaqt/services/link_service/link1_service.h>
#include <qindaqt/services/link_service/qt_notification_presenter.h>

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
    QCommandLineOption recordingNotifier(QStringLiteral("recording-notifier"),
                                         QStringLiteral("Test only: record phone notifications to FILE instead of "
                                                        "posting them to the notification server."),
                                         QStringLiteral("file"));
    QCommandLineOption configDir(QStringLiteral("config-dir"), QStringLiteral("Settings directory."), QStringLiteral("dir"));
    QCommandLineOption cacheDir(QStringLiteral("cache-dir"), QStringLiteral("Icon cache directory."), QStringLiteral("dir"));
    parser.addOptions({dataDir, port, noAdb, noNotify, recordingNotifier, configDir, cacheDir});
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
    Link1Service service(config, bus, presenter.get(), adb.get());
    QString error;
    if (!service.start(&error)) {
        qCritical("qindaqt-link-service: %s", qPrintable(error));
        return 1;
    }
    return app.exec();
}
