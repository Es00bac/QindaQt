// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_chooser_dialog.h"
#include "media_presenter.h"
#include "../../removable_media_client/media_source_fixture.h"
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
using namespace QindaQt::Services::Portal;
namespace Media = QindaQt::RemovableMedia;
class MediaChooserTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void initialObservationAndCloseDoNotMount() {
        MediaFixture::Source source; source.value.rows = {MediaFixture::volume("one"), MediaFixture::volume("two")};
        source.value.rows[1].partitionNumber = 2;
        FileChooserRequest request; request.folder = QDir::tempPath();
        FileChooserDialog dialog(request, &source); dialog.markReady();
        auto *devices = dialog.findChild<QListWidget *>("portalMediaDevices"); QVERIFY(devices);
        QCOMPARE(devices->count(), 2); QVERIFY(devices->item(0)->text().contains("<b>"));
        QVERIFY(devices->item(1)->text().contains("partition 2"));
        QCOMPARE(source.writes, 0); dialog.reject(); QCOMPARE(source.writes, 0);
    }
    void openWaitsAndCloseWithdrawsLateMountNavigation() {
        QTemporaryDir temp; const auto root = temp.filePath("device"); QVERIFY(QDir().mkpath(root));
        MediaFixture::Source source; source.value.rows = {MediaFixture::volume("one")};
        ChooserMediaPresenter presenter(source, false); presenter.setLocation(temp.path());
        QSignalSpy navigate(&presenter, &ChooserMediaPresenter::navigateRequested);
        presenter.open("one"); QCOMPARE(source.writes, 1); QCOMPARE(navigate.count(), 0);
        MediaFixture::setMounted(source.value.rows[0], root); ++source.value.lineage.revision; source.publish();
        QCOMPARE(navigate.count(), 0); presenter.closeInterest(); source.finish(); QCOMPARE(navigate.count(), 0);
    }
    void readOnlyBlocksSaveAndSaveManyButAllowsOpen() {
        QTemporaryDir temp; MediaFixture::Source source;
        auto row = MediaFixture::volume("one", temp.path()); row.readOnly = Media::ReadOnlyState::ReadOnly;
        source.value.rows = {row};
        for (auto mode : {FileChooserMode::Open, FileChooserMode::Save, FileChooserMode::SaveMany}) {
            FileChooserRequest request; request.folder = temp.path(); request.mode = mode;
            request.directory = mode == FileChooserMode::SaveMany; request.files = {"one.txt", "two.txt"};
            FileChooserDialog dialog(request, &source); dialog.markReady();
            const auto *accept = dialog.findChild<QPushButton *>("portalChooserAccept"); QVERIFY(accept);
            QCOMPARE(accept->isEnabled(), mode == FileChooserMode::Open);
            if (mode != FileChooserMode::Open) {
                auto *name = dialog.findChild<QLineEdit *>("portalFilename"); QVERIFY(name); name->setText("one.txt");
                dialog.accept(); QCOMPARE(dialog.response(), quint32(1)); QVERIFY(dialog.results().isEmpty());
            }
        }
        QCOMPARE(source.writes, 0);
    }
    void lostSelectionCannotReturnOldUriAfterOwnerReplacement() {
        QTemporaryDir temp; QFile file(temp.filePath("selected.txt")); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        MediaFixture::Source source; source.value.rows = {MediaFixture::volume("one", temp.path())};
        FileChooserRequest request; request.folder = temp.path(); request.currentName = "selected.txt";
        FileChooserDialog dialog(request, &source); dialog.markReady();
        auto *name = dialog.findChild<QLineEdit *>("portalFilename"); QVERIFY(name); name->setText("selected.txt");
        source.value.rows.clear(); ++source.value.lineage.revision; source.publish();
        const auto *accept = dialog.findChild<QPushButton *>("portalChooserAccept"); QVERIFY(accept); QVERIFY(!accept->isEnabled());
        QVERIFY(name->text().isEmpty()); source.value.rows = {MediaFixture::volume("replacement", temp.path())};
        source.value.lineage.owner = ":1.1000"; source.value.lineage.epoch = "replacement_epoch"; ++source.value.lineage.revision; source.publish();
        QVERIFY(!accept->isEnabled()); name->setText("selected.txt"); dialog.accept();
        QCOMPARE(dialog.response(), quint32(1)); QVERIFY(dialog.results().isEmpty());
    }
    void confirmedMountOpensAndExplicitFolderChangeRetiresPendingInterest() {
        QTemporaryDir temp; const auto root = temp.filePath("device"); QVERIFY(QDir().mkpath(root));
        MediaFixture::Source source; source.value.rows = {MediaFixture::volume("one")};
        ChooserMediaPresenter presenter(source, false); presenter.setLocation(temp.path());
        QSignalSpy navigate(&presenter, &ChooserMediaPresenter::navigateRequested);
        presenter.request("one", Media::Action::MountReadOnly, true);
        MediaFixture::setMounted(source.value.rows[0], root, Media::ReadOnlyState::ReadOnly);
        ++source.value.lineage.revision; source.publish(); source.finish(); QCOMPARE(navigate.count(), 1);
        QCOMPARE(navigate.first().first().toString(), root);
        source.value.rows = {MediaFixture::volume("two")}; ++source.value.lineage.revision; source.publish();
        presenter.open("two"); presenter.setLocation(root);
        MediaFixture::setMounted(source.value.rows[0], temp.path()); ++source.value.lineage.revision; source.publish(); source.finish();
        QCOMPARE(navigate.count(), 1); QCOMPARE(source.writes, 2);
    }
};
QTEST_MAIN(MediaChooserTests)
#include "tst_media_chooser.moc"
