// SPDX-License-Identifier: GPL-3.0-or-later
#include "fakes.h"
#include "model/local_directory_lister.h"
#include "model/navigation_controller.h"
#include "runtime/folder_navigations.h"
#include "runtime/media_presenter.h"
#include "../../services/removable_media_client/media_source_fixture.h"
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTest>
using namespace QindaQt::Apps::FileManager;
namespace {
std::unique_ptr<NavigationController> navigation() {
    return std::make_unique<NavigationController>(std::make_unique<LocalDirectoryLister>(), std::make_unique<Test::FakeFileLauncher>());
}
}
class MediaUiTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void deviceRowsFitAndExposeLiteralKeyboardActions_data() {
        QTest::addColumn<int>("width");
        QTest::newRow("compact") << 108;
        QTest::newRow("desktop") << 156;
    }
    void deviceRowsFitAndExposeLiteralKeyboardActions() {
        QFETCH(int, width);
        QTemporaryDir temp; MediaFixture::Source source;
        source.value.rows = {MediaFixture::volume("one", temp.path())};
        source.value.rows[0].displayName = QStringLiteral("<b>A long duplicate device label which must remain literal</b>");
        auto first = navigation(); first->navigateTo(temp.path());
        FolderNavigations tabs(*first, [] { return navigation(); }, [](auto &, auto &) {});
        MediaPresenter presenter(source, tabs);
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(QINDAQT_SOURCE_DIR)
            + QStringLiteral("/src/apps/file_manager/ui/MediaDeviceSection.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        const auto properties = QVariantMap{{"presenter", QVariant::fromValue(static_cast<QObject *>(&presenter))}};
        QQuickWindow window;
        std::unique_ptr<QObject> section(component.createWithInitialProperties(properties));
        QVERIFY2(section, qPrintable(component.errorString()));
        auto *item = qobject_cast<QQuickItem *>(section.get()); QVERIFY(item);
        window.resize(width, 700); item->setParentItem(window.contentItem());
        item->setWidth(width); item->setHeight(item->implicitHeight()); window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window)); QTRY_VERIFY(item->implicitHeight() > 0);
        for (const auto &name : {"mediaOpen_one", "mediaReadOnly_one", "mediaUnmount_one", "mediaRemove_one", "mediaDetails_one"}) {
            auto *button = section->findChild<QQuickItem *>(QString::fromLatin1(name)); QVERIFY(button);
            QTRY_VERIFY(button->width() > 0);
            const auto left = button->mapToItem(item, QPointF(0, 0)).x();
            QVERIFY2(left >= -0.5 && left + button->width() <= width + 0.5, name);
            const auto content = button->property("contentItem").value<QObject *>();
            QVERIFY(content); QCOMPARE(content->property("textFormat").toInt(), 0);
        }
        auto *open = section->findChild<QQuickItem *>("mediaOpen_one"); QVERIFY(open);
        QCOMPARE(open->property("text").toString(), QStringLiteral("<b>A long duplicate device label which must remain literal</b> · partition 1"));
        QCOMPARE(source.writes, 0); open->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(&window, Qt::Key_Tab);
        auto *unmount = section->findChild<QQuickItem *>("mediaUnmount_one"); QVERIFY(unmount);
        QTRY_VERIFY(unmount->hasActiveFocus());
        QTest::keyClick(&window, Qt::Key_Space); QCOMPARE(source.writes, 1);
        QCOMPARE(source.requests.back().action, QindaQt::RemovableMedia::Action::Unmount);
        item->setParentItem(nullptr);
    }
    void absentPresenterLoadsWithoutWarningsOrActions() {
        QQmlEngine engine;
        QQmlComponent component(&engine, QUrl::fromLocalFile(QStringLiteral(QINDAQT_SOURCE_DIR)
            + QStringLiteral("/src/apps/file_manager/ui/MediaDeviceSection.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));
        std::unique_ptr<QObject> section(component.create()); QVERIFY(section);
        QVERIFY(!section->property("visible").toBool());
    }
};
QTEST_MAIN(MediaUiTests)
#include "tst_media_ui.moc"
