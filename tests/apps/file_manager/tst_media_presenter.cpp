// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "runtime/folder_navigations.h"
#include "runtime/media_presenter.h"
#include "trash_test_support.h"
#include "../../services/removable_media_client/media_source_fixture.h"
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
using namespace QindaQt::Apps::FileManager;
namespace Media = QindaQt::RemovableMedia;
namespace {
std::unique_ptr<NavigationController> navigation() {
    return std::make_unique<NavigationController>(std::make_unique<LocalDirectoryLister>(), std::make_unique<Test::FakeFileLauncher>());
}
struct Harness {
    Harness() : first(navigation()), tabs(*first, [] { return navigation(); }, [](auto &, auto &) {}), presenter(source, tabs) {
        first->navigateTo(temp.path());
    }
    QTemporaryDir temp;
    MediaFixture::Source source;
    std::unique_ptr<NavigationController> first;
    FolderNavigations tabs;
    MediaPresenter presenter;
};
}
class MediaPresenterTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void volumeTrashReadOnlyDiscoveryAndUnavailableDoesNotMount() {
        Harness h;
        QTemporaryDir volume(QStringLiteral("/dev/shm/qindaqt-ed06-media-XXXXXX"));
        QVERIFY(volume.isValid());
        MutationResult admission;
        const auto root = volume.filePath(QStringLiteral(".Trash-") + QString::number(::getuid()));
        QVERIFY(TrashStorage::open({root, volume.path(), false}, true, admission));
        h.source.value.rows = {MediaFixture::volume("one", volume.path())};
        h.source.value.rows[0].readOnly = Media::ReadOnlyState::ReadOnly; h.source.publish();
        QTRY_COMPARE_WITH_TIMEOUT(h.presenter.rows()[0].toMap()["trashLocations"].toList().size(), 1, 3000);
        const auto locations = h.presenter.rows()[0].toMap()["trashLocations"].toList();
        QCOMPARE(locations.size(), 1);
        const auto files = locations[0].toMap()["path"].toString();
        QCOMPARE(files, QDir(root).filePath("files"));
        h.presenter.openTrash("one", files);
        QTRY_COMPARE_WITH_TIMEOUT(h.first->currentPath(), files, 3000); QCOMPARE(h.source.writes, 0);
        h.presenter.openTrash("one", h.temp.path());
        QCOMPARE(h.first->currentPath(), files); QCOMPARE(h.source.writes, 0);
        h.source.value.rows.clear(); ++h.source.value.lineage.revision; h.source.publish();
        QVERIFY(h.first->mediaLocationRevoked());
        h.presenter.openTrash("one", files); QCOMPARE(h.source.writes, 0);
    }
    void absentVolumeTrashDiscoveryCreatesNothing() {
        Harness h; const auto root = h.temp.filePath("device");
        QVERIFY(TrashTest::privateDir(root));
        h.source.value.rows = {MediaFixture::volume("one", root)}; h.source.publish();
        QVERIFY(h.presenter.rows()[0].toMap()["trashLocations"].toList().isEmpty());
        const auto privateRoot = QDir(root).filePath(QStringLiteral(".Trash-") + QString::number(::getuid()));
        QVERIFY(!QFileInfo::exists(privateRoot)); QVERIFY(!QFileInfo::exists(QDir(root).filePath(".Trash")));
        h.presenter.openTrash("one", QDir(privateRoot).filePath("files"));
        QCOMPARE(h.first->currentPath(), h.temp.path()); QCOMPARE(h.source.writes, 0);
    }

    void observationAndDuplicateRowsDoNotMount() {
        Harness h; h.source.value.rows = {MediaFixture::volume("one"), MediaFixture::volume("two")};
        h.source.value.rows[1].partitionNumber = 2; h.source.publish();
        QCOMPARE(h.presenter.rows().size(), 2); QCOMPARE(h.source.writes, 0);
        QVERIFY(h.presenter.rows()[0].toMap().value("name") != h.presenter.rows()[1].toMap().value("name"));
        h.presenter.refresh(); QCOMPARE(h.source.refreshes, 1); QCOMPARE(h.source.writes, 0);
    }
    void mountAndReadOnlyOpenWaitForConfirmedRoots() {
        Harness h; const auto root = h.temp.filePath("device"); QVERIFY(QDir().mkpath(root));
        h.source.value.rows = {MediaFixture::volume("one")}; h.source.publish();
        h.presenter.mountReadOnly("one"); QCOMPARE(h.source.writes, 1);
        QCOMPARE(h.source.requests.back().action, Media::Action::MountReadOnly);
        QCOMPARE(h.first->currentPath(), h.temp.path());
        MediaFixture::setMounted(h.source.value.rows[0], root, Media::ReadOnlyState::ReadOnly);
        ++h.source.value.lineage.revision; h.source.publish();
        QCOMPARE(h.first->currentPath(), h.temp.path());
        h.source.finish(); QCOMPARE(h.first->currentPath(), root);
    }
    void tabSwitchAndExplicitNavigationRetireOnlyDeferredOpen() {
        Harness h; const auto root = h.temp.filePath("device"); QVERIFY(QDir().mkpath(root));
        h.source.value.rows = {MediaFixture::volume("one")}; h.source.publish();
        h.presenter.open("one");
        auto *second = qobject_cast<NavigationController *>(h.tabs.create(h.temp.path()));
        h.tabs.setActive(second);
        MediaFixture::setMounted(h.source.value.rows[0], root); ++h.source.value.lineage.revision; h.source.publish(); h.source.finish();
        QCOMPARE(h.first->currentPath(), h.temp.path()); QCOMPARE(second->currentPath(), h.temp.path());
        QCOMPARE(h.source.writes, 1);
        h.source.value.rows = {MediaFixture::volume("two")}; ++h.source.value.lineage.revision; h.source.publish();
        h.presenter.open("two"); second->navigateTo(root);
        MediaFixture::setMounted(h.source.value.rows[0], h.temp.path()); ++h.source.value.lineage.revision; h.source.publish(); h.source.finish();
        QCOMPARE(second->currentPath(), root); QCOMPARE(h.source.writes, 2);
    }
    void attachmentLossWithdrawsBackgroundTabAndCannotReviveAtReusedPath() {
        Harness h; const auto root = h.temp.filePath("device"); QVERIFY(QDir().mkpath(root));
        QFile file(QDir(root).filePath("old.txt")); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        h.source.value.rows = {MediaFixture::volume("one", root)}; h.source.publish(); h.presenter.open("one");
        QVERIFY(h.first->entryCount() > 0);
        auto *second = h.tabs.create(h.temp.path()); h.tabs.setActive(second);
        h.source.value.rows.clear(); ++h.source.value.lineage.revision; h.source.publish();
        QVERIFY(h.first->mediaLocationRevoked()); QCOMPARE(h.first->entryCount(), 0); QVERIFY(!h.first->folderViewActive());
        const auto generation = h.first->listingGeneration();
        h.source.value.rows = {MediaFixture::volume("replacement", root)};
        h.source.value.lineage.owner = QStringLiteral(":1.1000"); h.source.value.lineage.epoch = QStringLiteral("replacement_epoch");
        ++h.source.value.lineage.revision; h.source.publish();
        h.first->refresh(); h.first->showGuestListing({}, "late old search");
        QVERIFY(h.first->mediaLocationRevoked()); QCOMPARE(h.first->entryCount(), 0); QCOMPARE(h.first->listingGeneration(), generation);
        h.tabs.setActive(h.first.get()); h.presenter.open("replacement");
        QVERIFY(!h.first->mediaLocationRevoked()); QVERIFY(h.first->entryCount() > 0);
    }
    void typedOrdinaryFolderNavigationEstablishesFreshInterest() {
        Harness h; const auto root = h.temp.filePath("device"); QVERIFY(QDir().mkpath(root));
        h.source.value.rows = {MediaFixture::volume("one", root)}; h.source.publish();
        h.first->navigateTo(root); h.source.value.rows.clear(); ++h.source.value.lineage.revision; h.source.publish();
        QVERIFY(h.first->mediaLocationRevoked());
        h.first->navigateTo(h.temp.path()); QVERIFY(!h.first->mediaLocationRevoked());
    }
    void nestedMountLossBindsMostSpecificAttachment_data() {
        QTest::addColumn<bool>("reverse");
        QTest::newRow("parent-first") << false;
        QTest::newRow("child-first") << true;
    }
    void nestedMountLossBindsMostSpecificAttachment() {
        QFETCH(bool, reverse);
        Harness h; const auto child = h.temp.filePath("child"); QVERIFY(QDir().mkpath(child));
        QFile file(QDir(child).filePath("selected.txt")); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        const auto parentRow = MediaFixture::volume("parent", h.temp.path());
        const auto childRow = MediaFixture::volume("child", child);
        h.source.value.rows = reverse ? QList<Media::VolumeRow>{childRow, parentRow} : QList<Media::VolumeRow>{parentRow, childRow};
        h.source.publish(); h.first->navigateTo(child); QVERIFY(h.first->entryCount() > 0);
        h.source.value.rows = {parentRow}; ++h.source.value.lineage.revision; h.source.publish();
        QVERIFY(h.first->mediaLocationRevoked()); QCOMPARE(h.first->entryCount(), 0);
        h.source.value.rows.append(MediaFixture::volume("replacement", child)); ++h.source.value.lineage.revision; h.source.publish();
        h.first->refresh(); QVERIFY(h.first->mediaLocationRevoked()); QCOMPARE(h.first->entryCount(), 0);
    }
    void createdInactiveControllerIsFencedBeforeItsFirstActivation() {
        Harness h; const auto root = h.temp.filePath("device"); QVERIFY(QDir().mkpath(root));
        QFile file(QDir(root).filePath("selected.txt")); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        h.source.value.rows = {MediaFixture::volume("one", root)}; h.source.publish(); h.presenter.open("one");
        auto *right = qobject_cast<NavigationController *>(h.tabs.create(root)); QVERIFY(right);
        QVERIFY(h.tabs.active() != right); QVERIFY(right->entryCount() > 0);
        h.source.value.rows.clear(); ++h.source.value.lineage.revision; h.source.publish();
        QVERIFY(right->mediaLocationRevoked()); QCOMPARE(right->entryCount(), 0);
        h.source.value.rows = {MediaFixture::volume("replacement", root)}; ++h.source.value.lineage.revision; h.source.publish();
        right->refresh(); QVERIFY(right->mediaLocationRevoked()); QCOMPARE(right->entryCount(), 0);
        h.tabs.setActive(right); QVERIFY(right->mediaLocationRevoked()); QCOMPARE(right->entryCount(), 0);
    }
    void cancelledOrUncertainRemovalNeverClaimsSafeUnplug() {
        Harness h; h.source.value.rows = {MediaFixture::volume("one")}; h.source.publish();
        h.presenter.remove("one"); h.source.finish(Media::OperationStatus::Uncertain);
        QVERIFY(!h.presenter.notice().contains("Safe to unplug"));
        h.presenter.remove("one"); h.source.finish(Media::OperationStatus::Applied, Media::RemovalMode::PoweredOff);
        QVERIFY(h.presenter.notice().contains("Safe to unplug"));
    }
};
QTEST_GUILESS_MAIN(MediaPresenterTests)
#include "tst_media_presenter.moc"
