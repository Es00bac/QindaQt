// SPDX-License-Identifier: GPL-3.0-or-later

#include "application_scanner.h"
#include "launcher_runtime_test_support.h"

#include <QSignalSpy>
#include <QtTest>

using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::ShellLauncher;
using namespace QindaQt::Tests::Launcher;

namespace {

void writeEntry(const QString &root, const QString &relative,
                const QString &name, const QString &extra = {})
{
    QVERIFY2(writeDesktopFile(root, relative, minimalEntry(name, QStringLiteral("true"), extra)),
             qPrintable(root + relative));
}

} // namespace

class ApplicationScannerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void refusesAnEmptyRootList();
    void scansInjectedRootsInPrecedenceOrder();
    void mapsSubdirectoriesIntoDesktopIds();
    void degradesOnHostileAndUnreadableEntries();
    void hiddenHigherPrecedenceEntryShadowsLowerRoot();
    void missingApplicationsTreeIsNormalNotDegraded();
    void watcherRefreshRepublishesWithFencedGeneration();
    void retainsDocumentsForTheExecutionAdapter();
    void orderingIsDeterministic();
};

void ApplicationScannerTests::refusesAnEmptyRootList()
{
    ApplicationScanner scanner({});
    QString error;
    QVERIFY(!scanner.start(&error));
    QVERIFY(!error.isEmpty());
}

void ApplicationScannerTests::scansInjectedRootsInPrecedenceOrder()
{
    QTemporaryDir rootA;
    QTemporaryDir rootB;
    QVERIFY(rootA.isValid() && rootB.isValid());
    writeEntry(rootA.path(), QStringLiteral("shared.desktop"), QStringLiteral("Alpha"));
    writeEntry(rootB.path(), QStringLiteral("shared.desktop"), QStringLiteral("Beta"));
    writeEntry(rootB.path(), QStringLiteral("other.desktop"), QStringLiteral("Gamma"));

    ApplicationScanner scanner({ rootA.path(), rootB.path() });
    QVERIFY(scanner.start());
    QVERIFY(scanner.catalog().has_value());
    const auto &entries = scanner.catalog()->entries();
    QCOMPARE(entries.size(), 2);
    // The first root claims the shared id; the second root's copy is a
    // duplicate diagnostic, never an override.
    const auto shared = scanner.catalog()->entry(QStringLiteral("shared"));
    QVERIFY(shared.has_value());
    QCOMPARE(shared->name, QStringLiteral("Alpha"));
    QCOMPARE(scanner.catalog()->diagnostics().size(), 1);
    QCOMPARE(scanner.catalog()->diagnostics().constFirst().kind,
             DiagnosticKind::DuplicateEntryId);
    QVERIFY(scanner.catalog()->entry(QStringLiteral("other")).has_value());
}

void ApplicationScannerTests::mapsSubdirectoriesIntoDesktopIds()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeEntry(root.path(), QStringLiteral("tools/editor.desktop"),
               QStringLiteral("Editor"));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    QVERIFY(scanner.catalog()->entry(QStringLiteral("tools-editor")).has_value());
}

void ApplicationScannerTests::degradesOnHostileAndUnreadableEntries()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeEntry(root.path(), QStringLiteral("good.desktop"), QStringLiteral("Good"));
    // Malformed document: no [Desktop Entry] group.
    QVERIFY(writeDesktopFile(root.path(), QStringLiteral("broken.desktop"),
                             QStringLiteral("[NotDesktop]\nName=Broken\n")));
    // Oversized document: beyond the byte ceiling.
    const QString huge = minimalEntry(QStringLiteral("Huge"), QStringLiteral("true"))
        + QString(300000, QLatin1Char('#'));
    QVERIFY(writeDesktopFile(root.path(), QStringLiteral("huge.desktop"), huge));
    // Unreadable entry: a dangling symlink fails to open on every platform,
    // including tests running with elevated privileges.
    QVERIFY(QFile::link(root.path() + QStringLiteral("/applications/missing-target.desktop"),
                        root.path() + QStringLiteral("/applications/dangling.desktop")));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    const auto &catalog = scanner.catalog();
    QVERIFY(catalog.has_value());
    QCOMPARE(catalog->entries().size(), 1);
    QCOMPARE(catalog->entries().constFirst().id, QStringLiteral("good"));

    // Degraded truth: the malformed document is a catalog diagnostic, the
    // oversized and unreadable files are scanner diagnostics.
    QVERIFY(!catalog->diagnostics().isEmpty());
    QStringList diagnosticIds;
    for (const auto &diagnostic : scanner.scanDiagnostics())
        diagnosticIds.append(diagnostic.sourceId);
    QVERIFY(diagnosticIds.contains(QStringLiteral("huge")));
    QVERIFY(diagnosticIds.contains(QStringLiteral("dangling")));
}

void ApplicationScannerTests::hiddenHigherPrecedenceEntryShadowsLowerRoot()
{
    QTemporaryDir rootA;
    QTemporaryDir rootB;
    QVERIFY(rootA.isValid() && rootB.isValid());
    writeEntry(rootA.path(), QStringLiteral("ghost.desktop"), QStringLiteral("Ghost"),
               QStringLiteral("NoDisplay=true\n"));
    writeEntry(rootB.path(), QStringLiteral("ghost.desktop"), QStringLiteral("Visible Ghost"));

    ApplicationScanner scanner({ rootA.path(), rootB.path() });
    QVERIFY(scanner.start());
    // The higher-precedence NoDisplay marker claims the identity; the lower
    // root cannot resurrect the application.
    QVERIFY(!scanner.catalog()->entry(QStringLiteral("ghost")).has_value());
}

void ApplicationScannerTests::missingApplicationsTreeIsNormalNotDegraded()
{
    QTemporaryDir emptyRoot;
    QTemporaryDir root;
    QVERIFY(emptyRoot.isValid() && root.isValid());
    writeEntry(root.path(), QStringLiteral("one.desktop"), QStringLiteral("One"));

    ApplicationScanner scanner({ emptyRoot.path(), root.path() });
    QVERIFY(scanner.start());
    QVERIFY(scanner.scanDiagnostics().isEmpty());
    QCOMPARE(scanner.catalog()->entries().size(), 1);
}

void ApplicationScannerTests::watcherRefreshRepublishesWithFencedGeneration()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeEntry(root.path(), QStringLiteral("first.desktop"), QStringLiteral("First"));

    ApplicationScanner scanner({ root.path() });
    QSignalSpy changes(&scanner, &ApplicationScanner::catalogChanged);
    QVERIFY(scanner.start());
    QCOMPARE(changes.size(), 1);
    const quint64 initial = scanner.generation();
    QCOMPARE(scanner.catalog()->entries().size(), 1);

    writeEntry(root.path(), QStringLiteral("second.desktop"), QStringLiteral("Second"));
    QTRY_COMPARE_WITH_TIMEOUT(changes.size(), 2, 5000);
    const quint64 refreshed = scanner.generation();
    QVERIFY(refreshed > initial);
    QCOMPARE(scanner.catalog()->entries().size(), 2);
    // The signal payload carries the generation it published, so a consumer
    // can fence stale reactions.
    QCOMPARE(changes.constLast().constFirst().toULongLong(), refreshed);

    // Content edits to an existing file also refresh (files are watched, not
    // only their directories).
    writeEntry(root.path(), QStringLiteral("first.desktop"), QStringLiteral("First Renamed"));
    QTRY_COMPARE_WITH_TIMEOUT(changes.size(), 3, 5000);
    QVERIFY(scanner.generation() > refreshed);
    QCOMPARE(scanner.catalog()->entry(QStringLiteral("first"))->name,
             QStringLiteral("First Renamed"));
}

void ApplicationScannerTests::retainsDocumentsForTheExecutionAdapter()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeEntry(root.path(), QStringLiteral("editor.desktop"), QStringLiteral("Editor"),
               QStringLiteral("Exec=qindaqt-editor --new\n"));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    const auto text = scanner.documentText(QStringLiteral("editor"));
    QVERIFY(text.has_value());
    QVERIFY(text->contains(QStringLiteral("Exec=qindaqt-editor --new")));
    QVERIFY(scanner.documentPath(QStringLiteral("editor"))
                .endsWith(QStringLiteral("applications/editor.desktop")));
    QVERIFY(!scanner.documentText(QStringLiteral("unknown")).has_value());
    QVERIFY(scanner.documentPath(QStringLiteral("unknown")).isEmpty());
}

void ApplicationScannerTests::orderingIsDeterministic()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    writeEntry(root.path(), QStringLiteral("zeta.desktop"), QStringLiteral("zeta"));
    writeEntry(root.path(), QStringLiteral("Alpha.desktop"), QStringLiteral("Alpha"));
    writeEntry(root.path(), QStringLiteral("mid.desktop"), QStringLiteral("Middling"));

    ApplicationScanner scanner({ root.path() });
    QVERIFY(scanner.start());
    QStringList names;
    for (const auto &entry : scanner.catalog()->entries())
        names.append(entry.name);
    QCOMPARE(names, QStringList({ QStringLiteral("Alpha"), QStringLiteral("Middling"),
                                  QStringLiteral("zeta") }));
}

QTEST_MAIN(ApplicationScannerTests)
#include "tst_application_scanner.moc"
