// SPDX-License-Identifier: GPL-3.0-or-later
#include "application_fixtures.h"
#include "fakes.h"
#include "model/applications_controller.h"
#include "model/applications_place.h"
#include "model/applications_place_order.h"
#include "model/listing_order.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "model/navigation_history.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Apps::FileManager;
using QindaQt::Apps::FileManager::Test::RecordingLaunchSeams;
using QindaQt::Apps::FileManager::Test::writeSampleCatalog;

namespace {

// The production composition (main.cpp) over a fixture catalog: the window's
// NavigationController browses Applications through the two decorators, and
// files still reach the (fake) document launcher.
struct Window final
{
    explicit Window(const QString &dataRoot)
        : applications({dataRoot}, recorder.seams())
    {
        auto files = std::make_unique<Test::FakeFileLauncher>();
        fileLauncher = files.get();
        navigation = std::make_unique<NavigationController>(
            std::make_unique<ApplicationsDirectoryLister>(
                std::make_unique<LocalDirectoryLister>(),
                [this] { applications.refresh(); return applications.listing(); }),
            std::make_unique<ApplicationsFileLauncher>(
                std::move(files),
                [this](const QString &id) { return applications.open(id); }));
        order = std::make_unique<ApplicationsPlaceOrder>(*navigation);
    }

    [[nodiscard]] QStringList names() const
    {
        QStringList result;
        for (int index = 0; index < navigation->entryCount(); ++index) {
            result.append(navigation->entryAt(index)->name);
        }
        return result;
    }

    RecordingLaunchSeams recorder;
    ApplicationsController applications;
    Test::FakeFileLauncher *fileLauncher = nullptr;
    std::unique_ptr<NavigationController> navigation;
    std::unique_ptr<ApplicationsPlaceOrder> order;
};

} // namespace

// ADR-0262: Applications is browsed by the ordinary NavigationController, so
// sorting, filtering, grouping, history and activation are the folder ones.
class ApplicationsPlaceTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theLocationIsARootWithOneBreadcrumb();
    void browsesEveryApplicationAToZLikeAFolder();
    void groupsByCategoryThroughTheKindSort();
    void filtersAndLeavesLikeAnyFolder();
    void keepsItsOwnSortApartFromFolders();
    void activationRoutesRowsToTheControllerAndFilesToTheLauncher();
    void onlyApplicationRowsReachTheOpener();
};

void ApplicationsPlaceTests::theLocationIsARootWithOneBreadcrumb()
{
    const QString location = QStringLiteral("applications:");
    QVERIFY(!NavigationHistory::parentOf(location).has_value());
    const auto crumbs = NavigationHistory::breadcrumbFor(location);
    QCOMPARE(crumbs.size(), 1);
    QCOMPARE(crumbs.first().name, QStringLiteral("Applications"));
    QCOMPARE(crumbs.first().path, location);
    // Ordinary paths are untouched.
    QCOMPARE(NavigationHistory::parentOf(QStringLiteral("/usr/share")).value(),
             QStringLiteral("/usr"));
}

void ApplicationsPlaceTests::browsesEveryApplicationAToZLikeAFolder()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    Window window(root.path());
    QSignalSpy navigationChanged(window.navigation.get(), &NavigationController::navigationChanged);
    window.navigation->navigateTo(root.path());
    QVERIFY(!window.navigation->applicationsPlace());

    window.navigation->navigateTo(ApplicationsController::location());
    QVERIFY(window.navigation->applicationsPlace());
    QCOMPARE(window.navigation->currentPath(), QStringLiteral("applications:"));
    QCOMPARE(window.navigation->statusKey(), QStringLiteral("ready"));
    QVERIFY(!window.navigation->canGoUp());
    QVERIFY(window.navigation->canGoBack());
    const auto crumbs = window.navigation->breadcrumb();
    QCOMPARE(crumbs.size(), 1);
    QCOMPARE(crumbs.first().toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Applications"));
    // A to Z by default, case-insensitively, hidden entries absent.
    QCOMPARE(window.names(), (QStringList{QStringLiteral("anagram"), QStringLiteral("Bus App"),
                                          QStringLiteral("Console Tool"), QStringLiteral("Editor"),
                                          QStringLiteral("Zeta Viewer")}));
    QVERIFY(window.navigation->statusMessage().isEmpty());

    // What QML receives: app identity, icon, category as Kind, a dash for the
    // unknown size, and the launchable flag.
    const auto first = window.navigation->entries().first().toMap();
    QCOMPARE(first.value(QStringLiteral("applicationId")).toString(), QStringLiteral("game"));
    QCOMPARE(first.value(QStringLiteral("path")).toString(), QStringLiteral("applications:game"));
    QCOMPARE(first.value(QStringLiteral("kindText")).toString(), QStringLiteral("Games"));
    QCOMPARE(first.value(QStringLiteral("sizeText")).toString(), QStringLiteral("—"));
    QCOMPARE(first.value(QStringLiteral("iconName")).toString(),
             QStringLiteral("application-x-executable"));
    QVERIFY(first.value(QStringLiteral("launchable")).toBool());
    const auto console = window.navigation->entries().at(2).toMap();
    QVERIFY(!console.value(QStringLiteral("launchable")).toBool());
    QVERIFY(console.value(QStringLiteral("note")).toString().contains(QStringLiteral("terminal")));

    // Typed spellings fold onto the one location.
    window.navigation->navigateTo(root.path());
    window.navigation->navigateTo(QStringLiteral("applications:///"));
    QVERIFY(window.navigation->applicationsPlace());
    QCOMPARE(window.navigation->entryCount(), 5);
}

void ApplicationsPlaceTests::groupsByCategoryThroughTheKindSort()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    Window window(root.path());
    window.navigation->navigateTo(ApplicationsController::location());

    // Group by Category = the Kind sort: category A to Z, names A to Z inside.
    window.navigation->setSortColumn(QStringLiteral("kind"));
    QStringList kinds;
    for (int index = 0; index < window.navigation->entryCount(); ++index) {
        kinds.append(window.navigation->entryAt(index)->kindText);
    }
    QCOMPARE(kinds, (QStringList{QStringLiteral("Development"), QStringLiteral("Games"),
                                 QStringLiteral("Graphics"), QStringLiteral("System"),
                                 QStringLiteral("Utilities")}));

    // Within a category the name order is kept (two Utilities entries).
    QTemporaryDir more;
    QVERIFY(writeSampleCatalog(more.path()));
    QVERIFY(Test::writeDesktop(more.filePath(QStringLiteral("applications/abacus.desktop")),
                               Test::entryText(QStringLiteral("Abacus"), QStringLiteral("/usr/bin/abacus"),
                                               QStringLiteral("Utility;"))));
    Window grouped(more.path());
    grouped.navigation->navigateTo(ApplicationsController::location());
    grouped.navigation->setSortColumn(QStringLiteral("kind"));
    QCOMPARE(grouped.names().mid(4), (QStringList{QStringLiteral("Abacus"), QStringLiteral("Bus App")}));

    // The pure ordering rule the grouping relies on.
    DirectoryEntry a;
    a.name = QStringLiteral("Zed");
    a.kindText = QStringLiteral("Development");
    DirectoryEntry b;
    b.name = QStringLiteral("Alpha");
    b.kindText = QStringLiteral("Utilities");
    ListingOrder byKind;
    byKind.column = SortColumn::Kind;
    QVERIFY(listingEntryLessThan(a, b, byKind));
    QVERIFY(!listingEntryLessThan(b, a, byKind));
}

void ApplicationsPlaceTests::keepsItsOwnSortApartFromFolders()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    Window window(root.path());
    auto &navigation = *window.navigation;
    navigation.navigateTo(root.path());
    // A folder sorted newest first...
    navigation.setSortColumn(QStringLiteral("modified"));
    navigation.setSortColumn(QStringLiteral("modified"));
    QCOMPARE(navigation.sortDirection(), QStringLiteral("descending"));

    // ...does not reorder Applications, which opens A to Z.
    navigation.navigateTo(ApplicationsController::location());
    QCOMPARE(navigation.sortColumn(), QStringLiteral("name"));
    QCOMPARE(navigation.sortDirection(), QStringLiteral("ascending"));
    QCOMPARE(window.names().first(), QStringLiteral("anagram"));

    // Group by Category there does not leak into the folder...
    navigation.setSortColumn(QStringLiteral("kind"));
    navigation.goBack();
    QCOMPARE(navigation.sortColumn(), QStringLiteral("modified"));
    QCOMPARE(navigation.sortDirection(), QStringLiteral("descending"));

    // ...and is still on when the user comes back, through any route.
    navigation.navigateTo(QStringLiteral("applications:///"));
    QCOMPARE(navigation.sortColumn(), QStringLiteral("kind"));
    QCOMPARE(navigation.entryAt(0)->kindText, QStringLiteral("Development"));
    // A refresh inside the place keeps its order.
    navigation.refresh();
    QCOMPARE(navigation.sortColumn(), QStringLiteral("kind"));
    navigation.navigateTo(root.path());
    QCOMPARE(navigation.sortColumn(), QStringLiteral("modified"));

    // A window that opens straight into Applications starts A to Z.
    Window picker(root.path());
    picker.navigation->setSortColumn(QStringLiteral("size"));
    picker.navigation->navigateTo(ApplicationsController::location());
    QCOMPARE(picker.navigation->sortColumn(), QStringLiteral("name"));
}

void ApplicationsPlaceTests::filtersAndLeavesLikeAnyFolder()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    Window window(root.path());
    window.navigation->navigateTo(root.path());
    window.navigation->navigateTo(ApplicationsController::location());

    window.navigation->setNameFilter(QStringLiteral("TOOL"));
    QCOMPARE(window.names(), QStringList{QStringLiteral("Console Tool")});
    QCOMPARE(window.navigation->statusMessage(), QStringLiteral("1 matching item"));
    window.navigation->setNameFilter(QString());

    // Refresh rescans: an application installed meanwhile appears.
    QVERIFY(Test::writeDesktop(root.filePath(QStringLiteral("applications/new.desktop")),
                               Test::entryText(QStringLiteral("Newcomer"), QStringLiteral("/usr/bin/new"),
                                               QStringLiteral("Office;"))));
    window.navigation->refresh();
    QCOMPARE(window.navigation->entryCount(), 6);

    // Back returns to the folder; forward to Applications again.
    window.navigation->goBack();
    QVERIFY(!window.navigation->applicationsPlace());
    QCOMPARE(window.navigation->currentPath(), root.path());
    window.navigation->goForward();
    QVERIFY(window.navigation->applicationsPlace());
    QCOMPARE(window.navigation->entryCount(), 6);
}

void ApplicationsPlaceTests::activationRoutesRowsToTheControllerAndFilesToTheLauncher()
{
    QTemporaryDir root;
    QVERIFY(writeSampleCatalog(root.path()));
    QFile document(root.filePath(QStringLiteral("notes.txt")));
    QVERIFY(document.open(QIODevice::WriteOnly));
    document.write("notes");
    document.close();
    Window window(root.path());
    window.navigation->navigateTo(ApplicationsController::location());

    // Row 3 is "Editor": opening it asks the compositor, then spawns.
    window.navigation->activate(3);
    QCOMPARE(window.recorder.chosen, QStringList{QStringLiteral("editor")});
    QCOMPARE(window.recorder.spawned.size(), 1);
    QVERIFY(window.navigation->launchError().isEmpty());
    QVERIFY(window.fileLauncher->requestedPaths().isEmpty());

    // An inert row reports its limitation through the window's launch error.
    window.navigation->activate(2);
    QVERIFY(window.navigation->launchError().contains(QStringLiteral("terminal")));
    QCOMPARE(window.recorder.spawned.size(), 1);

    // A document in a folder still goes to ADR-0029's launcher, untouched.
    window.navigation->clearLaunchError();
    window.navigation->navigateTo(root.path());
    const int index = window.navigation->indexOfName(QStringLiteral("notes.txt"));
    QVERIFY(index >= 0);
    window.navigation->activate(index);
    QCOMPARE(window.fileLauncher->requestedPaths(),
             QStringList{root.filePath(QStringLiteral("notes.txt"))});
    QCOMPARE(window.recorder.chosen.size(), 2);
}

void ApplicationsPlaceTests::onlyApplicationRowsReachTheOpener()
{
    auto inner = std::make_unique<Test::FakeFileLauncher>();
    auto *files = inner.get();
    QStringList opened;
    ApplicationsFileLauncher launcher(std::move(inner), [&opened](const QString &id) {
        opened.append(id);
        return id == QStringLiteral("bad") ? QStringLiteral("refused") : QString();
    });
    QVERIFY(launcher.launch(QStringLiteral("applications:org.example.App")).ok());
    const auto refused = launcher.launch(QStringLiteral("applications:bad"));
    QCOMPARE(refused.error, LaunchError::LaunchFailed);
    QCOMPARE(refused.diagnostic, QStringLiteral("refused"));
    // The bare location and ordinary paths never reach the opener.
    QVERIFY(launcher.launch(QStringLiteral("applications:")).ok());
    QVERIFY(launcher.launch(QStringLiteral("/tmp/applications:x")).ok());
    QCOMPARE(opened, (QStringList{QStringLiteral("org.example.App"), QStringLiteral("bad")}));
    QCOMPARE(files->requestedPaths(),
             (QStringList{QStringLiteral("applications:"), QStringLiteral("/tmp/applications:x")}));

    Test::FakeDirectoryLister *folders = nullptr;
    auto fakeLister = std::make_unique<Test::FakeDirectoryLister>();
    folders = fakeLister.get();
    int served = 0;
    ApplicationsDirectoryLister lister(std::move(fakeLister), [&served] {
        ++served;
        ListingResult result;
        result.path = QStringLiteral("applications:");
        return result;
    });
    QVERIFY(lister.list(QStringLiteral("applications:")).ok());
    QVERIFY(!lister.list(QStringLiteral("/nowhere")).ok());
    QCOMPARE(served, 1);
    QCOMPARE(folders->requestedPaths(), QStringList{QStringLiteral("/nowhere")});
}

QTEST_GUILESS_MAIN(ApplicationsPlaceTests)
#include "tst_applications_place.moc"
