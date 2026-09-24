// SPDX-License-Identifier: GPL-3.0-or-later
#include "application_fixtures.h"
#include "model/applications_controller.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

using QindaQt::Apps::FileManager::ApplicationsController;
using QindaQt::Apps::FileManager::DirectoryEntry;
using QindaQt::Apps::FileManager::Test::RecordingLaunchSeams;
using QindaQt::Apps::FileManager::Test::entryText;
using QindaQt::Apps::FileManager::Test::writeDesktop;
using QindaQt::Apps::FileManager::Test::writeSampleCatalog;

namespace {

const DirectoryEntry *rowById(const QVector<DirectoryEntry> &rows, const QString &id)
{
    const auto match = std::find_if(rows.cbegin(), rows.cend(), [&id](const DirectoryEntry &row) {
        return row.applicationId == id;
    });
    return match == rows.cend() ? nullptr : &*match;
}

} // namespace

// ADR-0262: the controller owns the catalog, the flat listing, Get Info and
// the launch policy; NavigationController does the rest (see
// tst_applications_place.cpp for the browsing rows).
class ApplicationsControllerTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void listsEveryVisibleApplicationAsAFlatRowSet();
    void marksLaunchableAndInertRows();
    void describesAnApplicationForGetInfo();
    void opensThroughTheCompositorBeforeSpawning();
    void aDockedWindowIsReplacedInsteadOfSpawning();
    void inertAndMissingEntriesReportTypedLimitations();
    void chooserModeHandsEveryChoiceToTheCompositor();
    void productionSeamsFallBackToALocalLaunchWithoutACompositor();
};

void ApplicationsControllerTests::listsEveryVisibleApplicationAsAFlatRowSet()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    RecordingLaunchSeams recorder;
    ApplicationsController controller({root.path()}, recorder.seams());
    QVERIFY(!controller.ready());
    QSignalSpy catalogChanged(&controller, &ApplicationsController::catalogChanged);
    controller.refresh();
    QVERIFY(controller.ready());
    QCOMPARE(catalogChanged.count(), 1);
    QCOMPARE(controller.location(), QStringLiteral("applications:"));

    const auto listing = controller.listing();
    QVERIFY(listing.ok());
    QCOMPARE(listing.path, QStringLiteral("applications:"));
    QVERIFY(!listing.truncated);
    // One flat row per visible application: nested vendor subdirectories are
    // flattened into the id, and NoDisplay/Hidden entries never appear.
    QCOMPARE(listing.entries.size(), 5);
    QVERIFY(!rowById(listing.entries, QStringLiteral("handler")));
    QVERIFY(!rowById(listing.entries, QStringLiteral("gone")));

    const auto *editor = rowById(listing.entries, QStringLiteral("editor"));
    QVERIFY(editor);
    QCOMPARE(editor->name, QStringLiteral("Editor"));
    QCOMPARE(editor->absolutePath, QStringLiteral("applications:editor"));
    QCOMPARE(editor->iconName, QStringLiteral("accessories-text-editor"));
    // The Kind is the primary category group, even for an entry nested in an
    // additional-category folder of the shared tree (TextEditor).
    QCOMPARE(editor->kindText, QStringLiteral("Development"));
    QVERIFY(!editor->isDirectory && !editor->isHidden);
    QVERIFY(editor->note.isEmpty());

    const auto *zeta = rowById(listing.entries, QStringLiteral("vendor-zeta"));
    QVERIFY(zeta);
    QCOMPARE(zeta->kindText, QStringLiteral("Graphics"));
    // An entry without an icon still shows an application glyph.
    QCOMPARE(zeta->iconName, QStringLiteral("application-x-executable"));
    QCOMPARE(rowById(listing.entries, QStringLiteral("game"))->kindText, QStringLiteral("Games"));

    // An empty catalog is a valid, ready, empty listing.
    ApplicationsController empty(QStringList{}, recorder.seams());
    empty.refresh();
    QVERIFY(empty.ready());
    QVERIFY(empty.listing().ok());
    QVERIFY(empty.listing().entries.isEmpty());
}

void ApplicationsControllerTests::marksLaunchableAndInertRows()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    RecordingLaunchSeams recorder;
    ApplicationsController controller({root.path()}, recorder.seams());
    controller.refresh();

    auto rows = controller.listing().entries;
    QVERIFY(rowById(rows, QStringLiteral("editor"))->note.isEmpty());
    QVERIFY(rowById(rows, QStringLiteral("console"))->note.contains(QStringLiteral("terminal")));
    QVERIFY(rowById(rows, QStringLiteral("bus"))->note.contains(QStringLiteral("D-Bus")));

    // In a picker every entry is choosable: the compositor owns the full
    // desktop-entry launch facility (ADR-0165).
    QSignalSpy catalogChanged(&controller, &ApplicationsController::catalogChanged);
    controller.setChooserMode(true);
    QCOMPARE(catalogChanged.count(), 1);
    rows = controller.listing().entries;
    QVERIFY(std::all_of(rows.cbegin(), rows.cend(),
                        [](const DirectoryEntry &row) { return row.note.isEmpty(); }));
}

void ApplicationsControllerTests::describesAnApplicationForGetInfo()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    RecordingLaunchSeams recorder;
    ApplicationsController controller({root.path()}, recorder.seams());
    controller.refresh();

    const auto info = controller.describe(QStringLiteral("editor"));
    QCOMPARE(info.value(QStringLiteral("id")).toString(), QStringLiteral("editor"));
    QCOMPARE(info.value(QStringLiteral("name")).toString(), QStringLiteral("Editor"));
    QCOMPARE(info.value(QStringLiteral("comment")).toString(), QStringLiteral("Edits text"));
    QCOMPARE(info.value(QStringLiteral("category")).toString(), QStringLiteral("Development"));
    QCOMPARE(info.value(QStringLiteral("categories")).toString(),
             QStringLiteral("Development, TextEditor"));
    // The planned argv, display only, with the spaced argument quoted.
    QCOMPARE(info.value(QStringLiteral("command")).toString(),
             QStringLiteral("/usr/bin/editor --flag \"two words\""));
    QCOMPARE(info.value(QStringLiteral("desktopFilePath")).toString(),
             root.filePath(QStringLiteral("applications/editor.desktop")));
    QVERIFY(info.value(QStringLiteral("note")).toString().isEmpty());

    QVERIFY(controller.describe(QStringLiteral("console"))
                .value(QStringLiteral("note")).toString().contains(QStringLiteral("terminal")));
    QVERIFY(controller.describe(QStringLiteral("missing")).isEmpty());
    // Nothing is launched by describing.
    QVERIFY(recorder.chosen.isEmpty() && recorder.spawned.isEmpty());
}

void ApplicationsControllerTests::opensThroughTheCompositorBeforeSpawning()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    RecordingLaunchSeams recorder;
    ApplicationsController controller({root.path()}, recorder.seams());
    controller.refresh();
    QSignalSpy succeeded(&controller, &ApplicationsController::chooserSucceeded);

    // Undocked: the compositor declines, so the planned argv is spawned once.
    QCOMPARE(controller.open(QStringLiteral("editor")), QString());
    QCOMPARE(recorder.chosen, QStringList{QStringLiteral("editor")});
    QCOMPARE(recorder.spawned.size(), 1);
    QCOMPARE(recorder.spawned.first(),
             (QStringList{QStringLiteral("/usr/bin/editor"), QStringLiteral("--flag"),
                          QStringLiteral("two words")}));
    QCOMPARE(succeeded.count(), 0);
    QVERIFY(controller.lastError().isEmpty());

    // A spawn refusal is reported and returned, never silent.
    recorder.spawnSucceeds = false;
    const QString failure = controller.open(QStringLiteral("game"));
    QCOMPARE(failure, QStringLiteral("Could not start 'anagram'"));
    QCOMPARE(controller.lastError(), failure);
    controller.clearLastError();
    QVERIFY(controller.lastError().isEmpty());
}

void ApplicationsControllerTests::aDockedWindowIsReplacedInsteadOfSpawning()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    RecordingLaunchSeams recorder;
    recorder.accept = true;
    ApplicationsController controller({root.path()}, recorder.seams());
    controller.refresh();
    QSignalSpy succeeded(&controller, &ApplicationsController::chooserSucceeded);

    // ADR-0172: the compositor accepted, so it launched the application into
    // this window's place; nothing is spawned locally, and the window is told
    // so it can close. Terminal entries work this way too.
    QCOMPARE(controller.open(QStringLiteral("console")), QString());
    QCOMPARE(recorder.chosen, QStringList{QStringLiteral("console")});
    QVERIFY(recorder.spawned.isEmpty());
    QCOMPARE(succeeded.count(), 1);
}

void ApplicationsControllerTests::inertAndMissingEntriesReportTypedLimitations()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    QVERIFY(writeDesktop(root.filePath(QStringLiteral("applications/broken.desktop")),
                         QStringLiteral("[Desktop Entry]\nType=Application\nName=Broken\n")));
    RecordingLaunchSeams recorder;
    ApplicationsController controller({root.path()}, recorder.seams());
    controller.refresh();

    QVERIFY(controller.open(QStringLiteral("console")).contains(QStringLiteral("terminal")));
    QVERIFY(controller.open(QStringLiteral("bus")).contains(QStringLiteral("D-Bus")));
    QVERIFY(controller.open(QStringLiteral("missing")).contains(QStringLiteral("installed")));
    // Every inert activation still asked the compositor first and spawned
    // nothing; an unknown id never reaches either seam.
    QCOMPARE(recorder.chosen, (QStringList{QStringLiteral("console"), QStringLiteral("bus")}));
    QVERIFY(recorder.spawned.isEmpty());
    if (controller.describe(QStringLiteral("broken")).isEmpty()) {
        // A document with no Exec is rejected by the catalog itself.
        QVERIFY(controller.open(QStringLiteral("broken")).contains(QStringLiteral("installed")));
    } else {
        QVERIFY(!controller.open(QStringLiteral("broken")).isEmpty());
    }
    QVERIFY(recorder.spawned.isEmpty());
}

void ApplicationsControllerTests::chooserModeHandsEveryChoiceToTheCompositor()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    RecordingLaunchSeams recorder;
    ApplicationsController controller({root.path()}, recorder.seams());
    controller.setChooserMode(true);
    controller.refresh();
    QSignalSpy succeeded(&controller, &ApplicationsController::chooserSucceeded);

    // ADR-0165: a rejected choice stays in the picker with the reason, and is
    // never turned into a local launch.
    QCOMPARE(controller.open(QStringLiteral("editor")), recorder.rejection);
    QCOMPARE(controller.lastError(), recorder.rejection);
    QVERIFY(recorder.spawned.isEmpty());
    QCOMPARE(succeeded.count(), 0);

    recorder.accept = true;
    QCOMPARE(controller.open(QStringLiteral("console")), QString());
    QCOMPARE(recorder.chosen,
             (QStringList{QStringLiteral("editor"), QStringLiteral("console")}));
    QVERIFY(recorder.spawned.isEmpty());
    QCOMPARE(succeeded.count(), 1);
    QVERIFY(controller.lastError().isEmpty());
}

// ADR-0172 routes an activation through the compositor first. That route must
// be a pure addition: with no compositor answering - a file manager run
// outside a QindaQt session, or before the compositor is up - activation
// still launches the application the old way. This row keeps the production
// seams, so it really spawns /usr/bin/true.
void ApplicationsControllerTests::productionSeamsFallBackToALocalLaunchWithoutACompositor()
{
    QTemporaryDir root;
    QVERIFY(writeDesktop(root.filePath(QStringLiteral("applications/spawn.desktop")),
                         entryText(QStringLiteral("Spawn"), QStringLiteral("/usr/bin/true"), {})));
    ApplicationsController controller({root.path()});
    controller.refresh();
    controller.activateEntry(QStringLiteral("spawn"));
    QVERIFY2(controller.lastError().isEmpty(), qPrintable(controller.lastError()));

    // The typed local limitations still apply on the fallback path.
    QVERIFY(writeDesktop(root.filePath(QStringLiteral("applications/dbusonly.desktop")),
                         QStringLiteral("[Desktop Entry]\nType=Application\n"
                                        "Name=DbusOnly\nExec=\nDBusActivatable=true\n")));
    controller.refresh();
    controller.activateEntry(QStringLiteral("dbusonly"));
    QVERIFY(!controller.lastError().isEmpty());
}

QTEST_GUILESS_MAIN(ApplicationsControllerTests)
#include "tst_applications_controller.moc"
