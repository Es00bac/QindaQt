// SPDX-License-Identifier: GPL-3.0-or-later
#include "wallpapercontroller.h"

#include "qindaqt/services/settings_client/qt_settings_transport.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_service/resident_settings_service.h"
#include "qindaqt/settings/settings_schema.h"

#include "shellpreferencevalues.h"

#include <QDBusConnection>
#include <QFile>
#include <QGuiApplication>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Shell;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsService;
using namespace QindaQt::Settings;

namespace {

// Windows the controller registered on the application: every top-level
// window in this process is one of its per-screen background surfaces.
QList<QQuickWindow *> backgroundWindows()
{
    QList<QQuickWindow *> windows;
    for (QWindow *window : QGuiApplication::allWindows()) {
        if (auto *quick = qobject_cast<QQuickWindow *>(window)) {
            windows.append(quick);
        }
    }
    return windows;
}

} // namespace

class WallpaperControllerTests final : public QObject {
    Q_OBJECT

private slots:
    // A fresh profile has no user-overrides file: every scoped key arrives
    // from the schema/profile default layers. The controller must apply the
    // bundled default, then live commits, on one window per screen.
    void freshProfileDefaultsAndConfirmedChangesReachBackgroundWindows();
};

void WallpaperControllerTests::freshProfileDefaultsAndConfirmedChangesReachBackgroundWindows()
{
    QProcess daemon;
    daemon.start(QStringLiteral(QINDAQT_DBUS_DAEMON_EXECUTABLE),
                 {QStringLiteral("--session"), QStringLiteral("--nofork"),
                  QStringLiteral("--print-address=1")});
    QVERIFY(daemon.waitForStarted());
    QVERIFY(daemon.waitForReadyRead());
    const QString address = QString::fromUtf8(daemon.readLine()).trimmed();
    const QString suffix = QString::number(QCoreApplication::applicationPid());
    const QString serviceConnection = QStringLiteral("wallpaper-service-") + suffix;
    const QString writerConnection = QStringLiteral("wallpaper-writer-") + suffix;
    const QString readerConnection = QStringLiteral("wallpaper-reader-") + suffix;
    auto serviceBus = QDBusConnection::connectToBus(address, serviceConnection);
    auto writerBus = QDBusConnection::connectToBus(address, writerConnection);
    auto readerBus = QDBusConnection::connectToBus(address, readerConnection);
    QVERIFY(serviceBus.isConnected());
    QVERIFY(writerBus.isConnected());
    QVERIFY(readerBus.isConnected());

    QString error;
    auto active = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v2.json"),
        nullptr, &error);
    auto legacy = SettingsSchema::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/schema-v1.json"),
        nullptr, &error, 1);
    QVERIFY2(active && legacy, qPrintable(error));
    QTemporaryDir storage;
    QVERIFY(storage.isValid());
    ResidentSettingsService service(
        serviceBus, *active, *legacy,
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/settings/profile-defaults/qindaqt.json"),
        storage.filePath(QStringLiteral("user.json")));
    QVERIFY(service.start().ok());

    QTemporaryDir dataRoot;
    QVERIFY(dataRoot.isValid());
    QVERIFY(QDir().mkpath(dataRoot.filePath(QStringLiteral("qindaqt/wallpapers"))));
    const auto plant = [](const QString &path) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write("image") > 0;
    };
    const QString bundled =
        dataRoot.filePath(QStringLiteral("qindaqt/wallpapers/qinda-punk.png"));
    QVERIFY(plant(bundled));
    const QString custom = storage.filePath(QStringLiteral("custom wallpaper.jpg"));
    QVERIFY(plant(custom));

    QtSettingsTransport transport(readerBus);
    SettingsClient client(transport, ShellPreferenceValues::scopedKeys(),
                          {.requestTimeoutMilliseconds = 500,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10, 20}});
    auto *app = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
    QVERIFY(app != nullptr);
    QQmlEngine engine;
    WallpaperController controller(*app, engine, client, {dataRoot.path()});
    controller.start();
    QVERIFY2(client.start(&error), qPrintable(error));

    // Fresh profile: the profile-default bundled identity applies without any
    // user-layer write. A missing wallpaperMode key would leave the source
    // empty here.
    QTRY_VERIFY_WITH_TIMEOUT(!backgroundWindows().isEmpty(), 2'000);
    QCOMPARE(backgroundWindows().size(), app->screens().size());
    QTRY_COMPARE_WITH_TIMEOUT(
        backgroundWindows().constFirst()->property("wallpaperSource").toUrl(),
        QUrl::fromLocalFile(bundled), 2'000);
    QCOMPARE(backgroundWindows().constFirst()->property("wallpaperMode").toString(),
             QStringLiteral("scaled"));

    // A confirmed custom path and mode from another client reconcile onto the
    // same surfaces.
    QtSettingsTransport writerTransport(writerBus);
    SettingsClient writer(writerTransport, ShellPreferenceValues::scopedKeys(),
                          {.requestTimeoutMilliseconds = 500,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10, 20}});
    QVERIFY2(writer.start(&error), qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(writer.state() == ClientState::Ready, 2'000);
    QVERIFY2(writer.setUserValue(QStringLiteral("appearance.wallpaper"), custom,
                                 &error),
             qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(!writer.writeInFlight(), 2'000);
    QVERIFY2(writer.setUserValue(QStringLiteral("appearance.wallpaperMode"),
                                 QStringLiteral("tiled"), &error),
             qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(!writer.writeInFlight(), 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(
        backgroundWindows().constFirst()->property("wallpaperSource").toUrl(),
        QUrl::fromLocalFile(custom), 4'000);
    QCOMPARE(backgroundWindows().constFirst()->property("wallpaperMode").toString(),
             QStringLiteral("tiled"));

    // An explicit empty choice means no wallpaper: the source clears and the
    // fallback color shows, with no window churn.
    QVERIFY2(writer.setUserValue(QStringLiteral("appearance.wallpaper"),
                                 QString(), &error),
             qPrintable(error));
    QTRY_VERIFY_WITH_TIMEOUT(!writer.writeInFlight(), 2'000);
    QTRY_COMPARE_WITH_TIMEOUT(
        backgroundWindows().constFirst()->property("wallpaperSource").toUrl(),
        QUrl{}, 4'000);
    QCOMPARE(backgroundWindows().size(), app->screens().size());

    service.stop();
    QDBusConnection::disconnectFromBus(serviceConnection);
    QDBusConnection::disconnectFromBus(writerConnection);
    QDBusConnection::disconnectFromBus(readerConnection);
    daemon.kill();
    QVERIFY(daemon.waitForFinished());
}

QTEST_MAIN(WallpaperControllerTests)
#include "tst_wallpapercontroller.moc"
