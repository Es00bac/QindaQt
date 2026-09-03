// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_scanner.h"
#include "launch_executor.h"
#include "launcher_runtime_test_support.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::Tests::Launcher;

class LaunchExecutorTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void spawnsPlannedArgvWithEntryPath();
    void refusesEntriesOutsideTheCatalog();
    void routesDBusActivatableEntries();
    void terminalEntriesNeedAWiredPolicy();
    void desktopActionsInheritEntryPolicy();
    void desktopActionsIgnoreActionPolicyLookalikes();
    void hostileExecNeverReachesTheSpawner();
    void spawnerFailureIsTruthful();
    void productionSpawnerRunsOnlyTheFixtureExecutable();
    void sanitizedEnvironmentDropsUnlistedVariables();
};

void LaunchExecutorTests::spawnsPlannedArgvWithEntryPath()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktopFile(
        root.path(), QStringLiteral("editor.desktop"),
        minimalEntry(QStringLiteral("Editor"), QStringLiteral("qindaqt-editor --new %u"),
                     QStringLiteral("Path=/srv/docs\n"))));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    RecordingSpawner spawner;
    RecordingActivator activator;
    LaunchExecutor executor(scanner, spawner, activator);

    const LaunchOutcome outcome = executor.launch(QStringLiteral("editor"));
    QCOMPARE(outcome.status, LaunchStatus::Spawned);
    QCOMPARE(spawner.requests.size(), 1);
    QCOMPARE(spawner.requests.constFirst().program, QStringLiteral("qindaqt-editor"));
    QCOMPARE(spawner.requests.constFirst().arguments,
             QStringList({ QStringLiteral("--new") }));
    QCOMPARE(spawner.requests.constFirst().workingDirectory, QStringLiteral("/srv/docs"));
    QCOMPARE(activator.activations.size(), 0);
}

void LaunchExecutorTests::refusesEntriesOutsideTheCatalog()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktopFile(root.path(), QStringLiteral("hidden.desktop"),
                             minimalEntry(QStringLiteral("Hidden"), QStringLiteral("true"),
                                          QStringLiteral("NoDisplay=true\n"))));
    QVERIFY(writeDesktopFile(root.path(), QStringLiteral("plain.desktop"),
                             minimalEntry(QStringLiteral("Plain"), QStringLiteral("true"))));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    RecordingSpawner spawner;
    RecordingActivator activator;
    LaunchExecutor executor(scanner, spawner, activator);

    QCOMPARE(executor.launch(QStringLiteral("missing")).status, LaunchStatus::Refused);
    // Hidden entries claim their identity and are not launchable.
    QCOMPARE(executor.launch(QStringLiteral("hidden")).status, LaunchStatus::Refused);
    QCOMPARE(executor.launch(QStringLiteral("plain"), QStringLiteral("no-action")).status,
             LaunchStatus::Refused);
    QCOMPARE(spawner.requests.size(), 0);
    QCOMPARE(activator.activations.size(), 0);
}

void LaunchExecutorTests::routesDBusActivatableEntries()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktopFile(
        root.path(), QStringLiteral("org.qindaqt.editor.desktop"),
        minimalEntry(QStringLiteral("Editor"), QStringLiteral("qindaqt-editor"),
                     QStringLiteral("DBusActivatable=true\nActions=new-window;\n\n"
                                    "[Desktop Action new-window]\nName=New Window\n"
                                    "Exec=qindaqt-editor --new-window\n"))));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    RecordingSpawner spawner;
    RecordingActivator activator;
    LaunchExecutor executor(scanner, spawner, activator);
    QSignalSpy finished(&executor, &LaunchExecutor::activationFinished);

    const LaunchOutcome primary = executor.launch(QStringLiteral("org.qindaqt.editor"));
    QCOMPARE(primary.status, LaunchStatus::ActivationRequested);
    QCOMPARE(activator.activations.size(), 1);
    QCOMPARE(activator.activations.constFirst().desktopId,
             QStringLiteral("org.qindaqt.editor"));
    QVERIFY(activator.activations.constFirst().actionId.isEmpty());
    QCOMPARE(spawner.requests.size(), 0);

    const LaunchOutcome action = executor.launch(QStringLiteral("org.qindaqt.editor"),
                                                 QStringLiteral("new-window"));
    QCOMPARE(action.status, LaunchStatus::ActivationRequested);
    QCOMPARE(activator.activations.constLast().actionId,
             QStringLiteral("new-window"));

    // Completion truth is relayed, including failure.
    activator.finish(QStringLiteral("org.qindaqt.editor"), true);
    activator.finish(QStringLiteral("org.qindaqt.editor"), false,
                     QStringLiteral("service did not answer"));
    QCOMPARE(finished.size(), 2);
    QCOMPARE(finished.constLast().at(1).toBool(), false);
}

void LaunchExecutorTests::terminalEntriesNeedAWiredPolicy()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktopFile(root.path(), QStringLiteral("top.desktop"),
                             minimalEntry(QStringLiteral("Top"), QStringLiteral("top -b"),
                                          QStringLiteral("Terminal=true\n"))));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    RecordingSpawner spawner;
    RecordingActivator activator;

    // Without a wired terminal policy the launch is a truthful refusal, never
    // a shell fallback.
    LaunchExecutor unarmed(scanner, spawner, activator);
    const LaunchOutcome refused = unarmed.launch(QStringLiteral("top"));
    QCOMPARE(refused.status, LaunchStatus::Refused);
    QVERIFY(refused.diagnostic.contains(QStringLiteral("terminal")));
    QCOMPARE(spawner.requests.size(), 0);

    LaunchExecutor armed(scanner, spawner, activator,
                         { QStringLiteral("qindaqt-terminal"), QStringLiteral("--execute") });
    const LaunchOutcome routed = armed.launch(QStringLiteral("top"));
    QCOMPARE(routed.status, LaunchStatus::Spawned);
    QCOMPARE(spawner.requests.size(), 1);
    QCOMPARE(spawner.requests.constFirst().program, QStringLiteral("qindaqt-terminal"));
    QCOMPARE(spawner.requests.constFirst().arguments,
             QStringList({ QStringLiteral("--execute"), QStringLiteral("top"),
                           QStringLiteral("-b") }));
}

void LaunchExecutorTests::desktopActionsInheritEntryPolicy()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktopFile(
        root.path(), QStringLiteral("main-terminal.desktop"),
        minimalEntry(QStringLiteral("Main Terminal"), QStringLiteral("fixture"),
                     QStringLiteral("Terminal=true\nPath=/expected-main-path\n"
                                    "Actions=run;\n\n[Desktop Action run]\n"
                                    "Name=Run\nExec=fixture --action\n"))));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    RecordingSpawner spawner;
    RecordingActivator activator;
    LaunchExecutor executor(scanner, spawner, activator,
                            { QStringLiteral("terminal-fixture"),
                              QStringLiteral("--execute") });

    QCOMPARE(executor.launch(QStringLiteral("main-terminal"),
                             QStringLiteral("run")).status,
             LaunchStatus::Spawned);
    QCOMPARE(spawner.requests.size(), 1);
    QCOMPARE(spawner.requests.constFirst().program,
             QStringLiteral("terminal-fixture"));
    QCOMPARE(spawner.requests.constFirst().arguments,
             QStringList({ QStringLiteral("--execute"), QStringLiteral("fixture"),
                           QStringLiteral("--action") }));
    QCOMPARE(spawner.requests.constFirst().workingDirectory,
             QStringLiteral("/expected-main-path"));
}

void LaunchExecutorTests::desktopActionsIgnoreActionPolicyLookalikes()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktopFile(
        root.path(), QStringLiteral("hostile.desktop"),
        minimalEntry(QStringLiteral("Hostile Action"), QStringLiteral("fixture"),
                     QStringLiteral("Path=/entry-only\nActions=hostile;\n\n"
                                    "[Desktop Action hostile]\nName=Hostile\n"
                                    "Exec=fixture --action\nTerminal=true\n"
                                    "Path=../../action-outside\n"
                                    "DBusActivatable=true\n"))));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    RecordingSpawner spawner;
    RecordingActivator activator;
    LaunchExecutor executor(scanner, spawner, activator,
                            { QStringLiteral("terminal-fixture"),
                              QStringLiteral("--execute") });

    QCOMPARE(executor.launch(QStringLiteral("hostile"),
                             QStringLiteral("hostile")).status,
             LaunchStatus::Spawned);
    QCOMPARE(spawner.requests.size(), 1);
    QCOMPARE(activator.activations.size(), 0);
    QCOMPARE(spawner.requests.constFirst().program, QStringLiteral("fixture"));
    QCOMPARE(spawner.requests.constFirst().arguments,
             QStringList({ QStringLiteral("--action") }));
    QCOMPARE(spawner.requests.constFirst().workingDirectory,
             QStringLiteral("/entry-only"));
}

void LaunchExecutorTests::hostileExecNeverReachesTheSpawner()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    // Valid document, but its Exec embeds a list field code mid-token.
    QVERIFY(writeDesktopFile(root.path(), QStringLiteral("hostile.desktop"),
                             minimalEntry(QStringLiteral("Hostile"),
                                          QStringLiteral("fixture --uri=%U"))));
    // A document whose execution keys are malformed is unusable even though
    // the presentation model accepted it.
    QVERIFY(writeDesktopFile(root.path(), QStringLiteral("dup.desktop"),
                             minimalEntry(QStringLiteral("Dup"), QStringLiteral("one"))
                                 + QStringLiteral("Exec=two\n")));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    RecordingSpawner spawner;
    RecordingActivator activator;
    LaunchExecutor executor(scanner, spawner, activator);

    QCOMPARE(executor.launch(QStringLiteral("hostile")).status, LaunchStatus::Refused);
    QCOMPARE(executor.launch(QStringLiteral("dup")).status, LaunchStatus::Refused);
    QCOMPARE(spawner.requests.size(), 0);
}

void LaunchExecutorTests::spawnerFailureIsTruthful()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    QVERIFY(writeDesktopFile(root.path(), QStringLiteral("app.desktop"),
                             minimalEntry(QStringLiteral("App"), QStringLiteral("app"))));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    RecordingSpawner spawner;
    spawner.nextResult = { false, QStringLiteral("executable vanished") };
    RecordingActivator activator;
    LaunchExecutor executor(scanner, spawner, activator);

    const LaunchOutcome outcome = executor.launch(QStringLiteral("app"));
    QCOMPARE(outcome.status, LaunchStatus::Failed);
    QCOMPARE(outcome.diagnostic, QStringLiteral("executable vanished"));
}

void LaunchExecutorTests::productionSpawnerRunsOnlyTheFixtureExecutable()
{
    // AGENT-CONTRACT: The only real processes any launcher test starts are the
    // inert fixtures /bin/true and /bin/false; everything else uses the seam.
    QProcessLaunchSpawner spawner;
    const SpawnResult ok = spawner.spawn({ QStringLiteral("/bin/true"), {}, {} });
    QVERIFY2(ok.ok, qPrintable(ok.diagnostic));

    const SpawnResult absent = spawner.spawn({ QStringLiteral("/bin/false"), {}, {} });
    QVERIFY2(absent.ok, qPrintable(absent.diagnostic));

    const SpawnResult invalid = spawner.spawn({ QString(), {}, {} });
    QVERIFY(!invalid.ok);
}

void LaunchExecutorTests::sanitizedEnvironmentDropsUnlistedVariables()
{
    QProcessEnvironment base;
    base.insert(QStringLiteral("PATH"), QStringLiteral("/usr/bin"));
    base.insert(QStringLiteral("HOME"), QStringLiteral("/home/fixture"));
    base.insert(QStringLiteral("LC_MESSAGES"), QStringLiteral("C"));
    base.insert(QStringLiteral("QINDAQT_APPLET_DIR"), QStringLiteral("/tmp/poison"));
    base.insert(QStringLiteral("LD_PRELOAD"), QStringLiteral("/tmp/evil.so"));
    base.insert(QStringLiteral("PROMPT_COMMAND"), QStringLiteral("id"));

    const QProcessEnvironment sanitized = sanitizedChildEnvironment(base);
    QVERIFY(sanitized.contains(QStringLiteral("PATH")));
    QVERIFY(sanitized.contains(QStringLiteral("LC_MESSAGES")));
    QVERIFY(!sanitized.contains(QStringLiteral("QINDAQT_APPLET_DIR")));
    QVERIFY(!sanitized.contains(QStringLiteral("LD_PRELOAD")));
    QVERIFY(!sanitized.contains(QStringLiteral("PROMPT_COMMAND")));
}

QTEST_MAIN(LaunchExecutorTests)
#include "tst_launch_executor.moc"
