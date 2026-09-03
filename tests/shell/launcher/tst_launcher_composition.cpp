// SPDX-License-Identifier: GPL-3.0-or-later

#include "launcherappletcomposition.h"

#include "launcher_applet_controller.h"
#include "launcher_persistence.h"
#include "launcher_runtime_test_support.h"

#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applets/manifest_catalog.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QDBusConnection>
#include <QDir>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt;
using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Shell;
using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::Tests::Launcher;

namespace {

struct CatalogFixture {
    Applets::ManifestCatalog catalog;
    AppletHost::CapabilityPolicy policy;

    bool load(QString *error)
    {
        if (!catalog.loadDirectory(
                QStringLiteral(QINDAQT_SOURCE_DIR "/data/applets"), error)) {
            return false;
        }
        const auto loaded = AppletHost::CapabilityPolicyLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR
                           "/data/applet-policy/default.json"));
        if (!loaded.ok) {
            *error = loaded.error;
            return false;
        }
        policy = loaded.policy;
        return true;
    }
};

std::unique_ptr<QTemporaryDir> applicationRoot()
{
    return std::make_unique<QTemporaryDir>(QDir(QStringLiteral(
        QINDAQT_TEST_SCRATCH)).filePath(QStringLiteral("composition-XXXXXX")));
}

} // namespace

class LauncherCompositionTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void resolvesOnlyExplicitAbsoluteXdgRoots();
    void auditedCompositionUsesInjectedExecutionSeams();
    void deniedGrantNeverReachesExecutionSeams();
};

void LauncherCompositionTests::resolvesOnlyExplicitAbsoluteXdgRoots()
{
    QProcessEnvironment environment;
    environment.insert(QStringLiteral("XDG_DATA_HOME"),
                       QStringLiteral("/data/home"));
    environment.insert(QStringLiteral("XDG_DATA_DIRS"),
                       QStringLiteral("relative:/opt/share:/data/home:/usr/share"));
    QCOMPARE(launcherDataRoots(environment, QStringLiteral("/ignored/home")),
             QStringList({QStringLiteral("/data/home"),
                          QStringLiteral("/opt/share"),
                          QStringLiteral("/usr/share")}));

    QCOMPARE(launcherDataRoots(QProcessEnvironment(),
                               QStringLiteral("/users/beatrice")),
             QStringList({QStringLiteral("/users/beatrice/.local/share"),
                          QStringLiteral("/usr/local/share"),
                          QStringLiteral("/usr/share")}));
}

void LauncherCompositionTests::auditedCompositionUsesInjectedExecutionSeams()
{
    QVERIFY2(QDBusConnection::sessionBus().isConnected(),
             "composition test requires its private dbus-run-session bus");
    auto root = applicationRoot();
    QVERIFY(root->isValid());
    QVERIFY(writeDesktopFile(
        root->path(), QStringLiteral("editor.desktop"),
        minimalEntry(QStringLiteral("Fixture Editor"),
                     QStringLiteral("qindaqt-editor --safe"))));
    QVERIFY(writeDesktopFile(
        root->path(), QStringLiteral("org.fixture.Activated.desktop"),
        minimalEntry(QStringLiteral("Activated Fixture"),
                     QStringLiteral("ignored"),
                     QStringLiteral("DBusActivatable=true\n"))));

    CatalogFixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    FakeSettingsTransport transport;
    SettingsClient settings(
        transport, {LauncherPersistenceController::pinnedKey(),
                    LauncherPersistenceController::recentKey()});
    RecordingSpawner spawner;
    RecordingActivator activator;
    LauncherAppletComposition composition(
        fixture.catalog, fixture.policy, {root->path()}, settings, spawner,
        activator);
    QVERIFY2(composition.start(&error), qPrintable(error));
    QVERIFY(settings.start());
    transport.announceOwner();
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    transport.replyLastSnapshot(
        FakeSettingsTransport::snapshotWire(QStringLiteral("composition"), 0,
                                            {}));

    QTRY_COMPARE(composition.access()->phase(), QStringLiteral("ready"));
    QVERIFY(composition.access()->launchGranted());
    QVERIFY(composition.access()->activate(QStringLiteral("editor")));
    QCOMPARE(spawner.requests.size(), 1);
    QCOMPARE(spawner.requests.constFirst().program,
             QStringLiteral("qindaqt-editor"));
    QCOMPARE(spawner.requests.constFirst().arguments,
             QStringList({QStringLiteral("--safe")}));

    QVERIFY(composition.access()->activate(
        QStringLiteral("org.fixture.Activated")));
    QCOMPARE(activator.activations.size(), 1);
    QCOMPARE(activator.activations.constFirst().desktopId,
             QStringLiteral("org.fixture.Activated"));
    QCOMPARE(spawner.requests.size(), 1);
}

void LauncherCompositionTests::deniedGrantNeverReachesExecutionSeams()
{
    auto root = applicationRoot();
    QVERIFY(root->isValid());
    QVERIFY(writeDesktopFile(root->path(), QStringLiteral("editor.desktop"),
                             minimalEntry(QStringLiteral("Fixture Editor"),
                                          QStringLiteral("qindaqt-editor"))));
    CatalogFixture fixture;
    QString error;
    QVERIFY2(fixture.load(&error), qPrintable(error));
    fixture.policy.auditedBuiltinDefault =
        AppletHost::CapabilityDisposition::Deny;
    FakeSettingsTransport transport;
    SettingsClient settings(
        transport, {LauncherPersistenceController::pinnedKey(),
                    LauncherPersistenceController::recentKey()});
    RecordingSpawner spawner;
    RecordingActivator activator;
    LauncherAppletComposition composition(
        fixture.catalog, fixture.policy, {root->path()}, settings, spawner,
        activator);
    QVERIFY2(composition.start(&error), qPrintable(error));

    QVERIFY(!composition.access()->launchGranted());
    QVERIFY(!composition.access()->activate(QStringLiteral("editor")));
    QVERIFY(spawner.requests.isEmpty());
    QVERIFY(activator.activations.isEmpty());
}

QTEST_GUILESS_MAIN(LauncherCompositionTests)
#include "tst_launcher_composition.moc"
