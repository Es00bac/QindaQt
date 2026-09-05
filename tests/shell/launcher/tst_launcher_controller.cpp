// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_scanner.h"
#include "launch_executor.h"
#include "launcher_applet_controller.h"
#include "launcher_persistence.h"
#include "launcher_runtime_test_support.h"

#include <qindaqt/services/settings_client/settings_client.h>

#include <QScopeGuard>
#include <QtTest>

#include <sys/stat.h>
#include <unistd.h>

using namespace QindaQt::Services::SettingsClient;
using namespace QindaQt::Services::SettingsProtocol;
using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::Tests::Launcher;

namespace {

// Member declaration order is the construction order: the fixture root
// outlives the scanner, and the scanner exists before the executor that
// borrows it.
struct Stack {
    QTemporaryDir root;
    ApplicationScanner scanner;
    FakeSettingsTransport transport;
    SettingsClient client;
    LauncherPersistenceController persistence;
    RecordingSpawner spawner;
    RecordingActivator activator;
    LaunchExecutor executor;

    Stack()
        : scanner({ root.path() })
        , client(transport,
                 { LauncherPersistenceController::pinnedKey(),
                   LauncherPersistenceController::recentKey() })
        , persistence(client)
        , executor(scanner, spawner, activator)
    {
    }
};

void writeEntry(const QString &root, const QString &file, const QString &name,
                const QString &exec, const QString &extra = {})
{
    QVERIFY2(writeDesktopFile(root, file, minimalEntry(name, exec, extra)),
             qPrintable(file));
}

QVariantList allItems(const QVariantList &sections)
{
    QVariantList items;
    for (const QVariant &section : sections)
        items.append(section.toMap().value(QStringLiteral("items")).toList());
    return items;
}

} // namespace

class LauncherControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void projectsCatalogPinnedAndRecent();
    void queryCollapsesToSearchResults();
    void sectionsForQueryIsAPureProjection();
    void activationRequiresTheGrant();
    void activationLaunchesAndRecordsRecent();
    void individualEntryFailuresDoNotBlockUsableCatalog();
    void whollyInvalidCatalogHasActionableMessage();
    void rootAccessFailuresReachTheProjection();
    void inaccessibleAncestorReachesTheProjection();
    void nullCollaboratorsFailClosed();
};

void LauncherControllerTests::projectsCatalogPinnedAndRecent()
{
    Stack stack;
    QVERIFY(stack.root.isValid());
    writeEntry(stack.root.path(), QStringLiteral("editor.desktop"),
               QStringLiteral("Fixture Editor"), QStringLiteral("editor"),
               QStringLiteral("Categories=Utility\n"));
    QVERIFY(stack.scanner.start());

    LauncherAppletController controller(&stack.scanner, &stack.persistence,
                                        &stack.executor, true);
    QCOMPARE(controller.phase(), QStringLiteral("ready"));
    QVERIFY(controller.launchGranted());
    QCOMPARE(controller.diagnostic(), QString());

    const QVariantList sections = controller.sections();
    QVERIFY(!sections.isEmpty());
    const QVariantList items = allItems(sections);
    QCOMPARE(items.size(), 1);
    const auto item = items.constFirst().toMap();
    QCOMPARE(item.value(QStringLiteral("entryId")).toString(),
             QStringLiteral("editor"));
    QCOMPARE(item.value(QStringLiteral("displayText")).toString(),
             QStringLiteral("Fixture Editor"));
    QVERIFY(!item.value(QStringLiteral("accessibleDescription")).toString().isEmpty());
}

void LauncherControllerTests::queryCollapsesToSearchResults()
{
    Stack stack;
    QVERIFY(stack.root.isValid());
    writeEntry(stack.root.path(), QStringLiteral("editor.desktop"),
               QStringLiteral("Fixture Editor"), QStringLiteral("editor"));
    writeEntry(stack.root.path(), QStringLiteral("terminal.desktop"),
               QStringLiteral("Fixture Terminal"), QStringLiteral("terminal"));
    QVERIFY(stack.scanner.start());

    LauncherAppletController controller(&stack.scanner, &stack.persistence,
                                        &stack.executor, true);
    controller.setQuery(QStringLiteral("editor"));
    QCOMPARE(controller.query(), QStringLiteral("editor"));
    const QVariantList sections = controller.sections();
    QCOMPARE(sections.size(), 1);
    QCOMPARE(sections.constFirst().toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("searchResults"));
    QCOMPARE(allItems(sections).size(), 1);

    controller.setQuery(QStringLiteral("   "));
    QVERIFY(controller.sections().constFirst().toMap()
                .value(QStringLiteral("kind")).toString()
            != QStringLiteral("searchResults"));

    // Overlong queries are bounded at the model ceiling, never rejected.
    const QString huge = QString(200, QLatin1Char('x'));
    controller.setQuery(huge);
    QCOMPARE(controller.query().size(), 128);
}

void LauncherControllerTests::sectionsForQueryIsAPureProjection()
{
    Stack stack;
    QVERIFY(stack.root.isValid());
    writeEntry(stack.root.path(), QStringLiteral("editor.desktop"),
               QStringLiteral("Fixture Editor"), QStringLiteral("editor"));
    writeEntry(stack.root.path(), QStringLiteral("terminal.desktop"),
               QStringLiteral("Fixture Terminal"), QStringLiteral("terminal"));
    QVERIFY(stack.scanner.start());

    LauncherAppletController controller(&stack.scanner, &stack.persistence,
                                        &stack.executor, true);
    controller.setQuery(QStringLiteral("terminal"));
    QCOMPARE(allItems(controller.sections()).size(), 1);

    // AGENT-CONTRACT: other shell controls (quick launch, command search)
    // read the launcher through this projection without disturbing the
    // launcher popup's own query or sections.
    const QVariantList browse = controller.sectionsForQuery(QString());
    QCOMPARE(allItems(browse).size(), 2);
    const QVariantList searched = controller.sectionsForQuery(QStringLiteral("editor"));
    QCOMPARE(searched.size(), 1);
    QCOMPARE(searched.constFirst().toMap().value(QStringLiteral("kind")).toString(),
             QStringLiteral("searchResults"));
    QCOMPARE(allItems(searched).constFirst().toMap().value(QStringLiteral("entryId")).toString(),
             QStringLiteral("editor"));
    QCOMPARE(controller.query(), QStringLiteral("terminal"));
    QCOMPARE(allItems(controller.sections()).size(), 1);
    QCOMPARE(controller.sectionsForQuery(QString(200, QLatin1Char('x'))).size(),
             controller.sectionsForQuery(QString(128, QLatin1Char('x'))).size());
}

void LauncherControllerTests::activationRequiresTheGrant()
{
    Stack stack;
    QVERIFY(stack.root.isValid());
    writeEntry(stack.root.path(), QStringLiteral("app.desktop"),
               QStringLiteral("App"), QStringLiteral("app"));
    QVERIFY(stack.scanner.start());

    LauncherAppletController denied(&stack.scanner, &stack.persistence,
                                    &stack.executor, false);
    QVERIFY(!denied.activate(QStringLiteral("app")));
    QVERIFY(denied.feedback().contains(QStringLiteral("not granted")));
    QCOMPARE(stack.spawner.requests.size(), 0);

    LauncherAppletController granted(&stack.scanner, &stack.persistence,
                                     &stack.executor, true);
    QVERIFY(!granted.activate(QStringLiteral("unknown")));
    QVERIFY(!granted.feedback().isEmpty());
    QCOMPARE(stack.spawner.requests.size(), 0);
}

void LauncherControllerTests::activationLaunchesAndRecordsRecent()
{
    Stack stack;
    QVERIFY(stack.root.isValid());
    writeEntry(stack.root.path(), QStringLiteral("app.desktop"),
               QStringLiteral("App"), QStringLiteral("app --go"));
    QVERIFY(stack.scanner.start());

    QVERIFY(stack.client.start());
    stack.transport.announceOwner();
    QTRY_VERIFY(!stack.transport.snapshots.isEmpty());
    stack.transport.replyLastSnapshot(FakeSettingsTransport::snapshotWire(
        QStringLiteral("epoch-a"), 0, {}));
    QTRY_VERIFY(stack.persistence.persistenceReady());

    LauncherAppletController controller(&stack.scanner, &stack.persistence,
                                        &stack.executor, true);
    QVERIFY(controller.activate(QStringLiteral("app")));
    QCOMPARE(stack.spawner.requests.size(), 1);
    QCOMPARE(stack.spawner.requests.constFirst().arguments,
             QStringList({ QStringLiteral("--go") }));
    QVERIFY(controller.feedback().isEmpty());

    // The activation records recent use through Settings1.
    QTRY_VERIFY(!stack.transport.commits.isEmpty());
    const auto operation =
        stack.transport.commits.constLast().operations.constFirst().toMap();
    QCOMPARE(operation.value(QStringLiteral("key")).toString(),
             LauncherPersistenceController::recentKey());
}

void LauncherControllerTests::individualEntryFailuresDoNotBlockUsableCatalog()
{
    Stack stack;
    QVERIFY(stack.root.isValid());
    writeEntry(stack.root.path(), QStringLiteral("good.desktop"),
               QStringLiteral("Good"), QStringLiteral("good"));
    QVERIFY(writeDesktopFile(stack.root.path(), QStringLiteral("broken.desktop"),
                             QStringLiteral("not a desktop document")));
    QVERIFY(stack.scanner.start());

    LauncherAppletController controller(&stack.scanner, &stack.persistence,
                                        &stack.executor, true);
    QCOMPARE(controller.phase(), QStringLiteral("degraded"));
    QVERIFY(controller.diagnostic().isEmpty());
    QVERIFY(!stack.scanner.catalog()->diagnostics().isEmpty());
    QCOMPARE(stack.scanner.catalog()->diagnostics().constFirst().sourceId,
             QStringLiteral("broken"));
    // The remaining valid entries still project.
    QCOMPARE(allItems(controller.sections()).size(), 1);
}

void LauncherControllerTests::whollyInvalidCatalogHasActionableMessage()
{
    Stack stack;
    QVERIFY(stack.root.isValid());
    QVERIFY(writeDesktopFile(stack.root.path(), QStringLiteral("broken.desktop"),
                             QStringLiteral("[Desktop Entry]\nType=Application\nExec=broken\n")));
    QVERIFY(stack.scanner.start());
    LauncherAppletController controller(&stack.scanner, &stack.persistence,
                                        &stack.executor, true);
    QCOMPARE(controller.phase(), QStringLiteral("degraded"));
    QVERIFY(controller.sections().isEmpty());
    QVERIFY(controller.diagnostic().contains(QStringLiteral("No usable applications")));
    QVERIFY(controller.diagnostic().contains(QStringLiteral("Check")));
    QVERIFY(stack.scanner.catalog()->diagnostics().constFirst().message.contains(
        QStringLiteral("Name")));
}

void LauncherControllerTests::rootAccessFailuresReachTheProjection()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(QFile::link(root.path() + QStringLiteral("/missing-applications"),
                        root.path() + QStringLiteral("/applications")));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    LauncherAppletController controller(&scanner, nullptr, nullptr, false);
    QCOMPARE(controller.phase(), QStringLiteral("degraded"));
    QVERIFY(controller.diagnostic().contains(QStringLiteral("dangling")));
    QVERIFY(controller.sections().isEmpty());
}

void LauncherControllerTests::inaccessibleAncestorReachesTheProjection()
{
    if (::geteuid() == 0)
        QSKIP("root cannot reproduce ancestor traversal denial");

    QTemporaryDir fixture;
    QVERIFY(fixture.isValid());
    const QString blocked = fixture.path() + QStringLiteral("/blocked");
    const QString dataRoot = blocked + QStringLiteral("/data-root");
    QVERIFY(QDir().mkpath(dataRoot + QStringLiteral("/applications")));
    QCOMPARE(::chmod(QFile::encodeName(blocked).constData(), 0000), 0);
    const auto restorePermissions = qScopeGuard([&blocked] {
        ::chmod(QFile::encodeName(blocked).constData(), 0700);
    });

    ApplicationScanner scanner({ dataRoot });
    QVERIFY(scanner.start());
    LauncherAppletController controller(&scanner, nullptr, nullptr, false);
    QCOMPARE(controller.phase(), QStringLiteral("degraded"));
    QVERIFY(controller.diagnostic().contains(
        QStringLiteral("cannot be determined")));
    QVERIFY(controller.sections().isEmpty());
}

void LauncherControllerTests::nullCollaboratorsFailClosed()
{
    LauncherAppletController controller(nullptr, nullptr, nullptr, false);
    QCOMPARE(controller.phase(), QStringLiteral("unavailable"));
    QVERIFY(!controller.diagnostic().isEmpty());
    QVERIFY(!controller.launchGranted());
    QVERIFY(!controller.activate(QStringLiteral("anything")));
    QVERIFY(!controller.pin(QStringLiteral("anything")));
    QVERIFY(!controller.unpin(QStringLiteral("anything")));
    QVERIFY(!controller.clearRecent());
    QVERIFY(!controller.persistenceStatus().isEmpty());
    QVERIFY(controller.sections().isEmpty());
}

QTEST_GUILESS_MAIN(LauncherControllerTests)
#include "tst_launcher_controller.moc"
