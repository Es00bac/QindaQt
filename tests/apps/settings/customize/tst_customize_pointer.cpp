// SPDX-License-Identifier: GPL-3.0-or-later
#include "customize_test_support.h"
#include "qindaqt/apps/settings_customize/customize_editor_host.h"
#include "qindaqt/apps/settings_customize/customize_settings_model.h"
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/themes/theme_loader.h"
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Apps::SettingsCustomize;
using namespace QindaQt::Apps::SettingsCustomize::TestSupport;

namespace {
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name && root->isVisible()) {
        return root;
    }
    for (auto *child : root->childItems()) {
        if (auto *found = findItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}
QPoint center(QQuickItem *item)
{
    return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
}
void movePointer(QQuickView &view, QPoint from, QPoint to)
{
    for (int step = 1; step <= 12; ++step) {
        QTest::mouseMove(&view, from + (to - from) * step / 12, 10);
        QCoreApplication::processEvents();
    }
}
}

class CustomizePointerTests final : public QObject {
    Q_OBJECT
private slots:
    void pointerDeliverySurvivesPreviewReconstruction_data();
    void pointerDeliverySurvivesPreviewReconstruction();
};

void CustomizePointerTests::pointerDeliverySurvivesPreviewReconstruction_data()
{
    QTest::addColumn<int>("width");
    QTest::newRow("compact") << 720;
    QTest::newRow("wide") << 1080;
}

void CustomizePointerTests::pointerDeliverySurvivesPreviewReconstruction()
{
    QFETCH(int, width);
    auto store = temporaryStore(QStringLiteral("customize-pointer"));
    QVERIFY(store->isValid());
    SequenceTransport transport;
    Services::SettingsClient::SettingsClient client(transport, {QString(LayoutProfileSettingsKey)});
    CustomizeSettingsModel model(client, {profile()}, manifests(),
        [&](const Profiles::LayoutProfile &selected) {
            return std::make_unique<RepositoryCustomizeEditorHost>(
                selected, outputs(), manifests(), store->path());
        });
    QVERIFY(client.start());
    Q_EMIT transport.ownerChanged(QStringLiteral(":1.90"));
    QTRY_VERIFY(!transport.snapshots.isEmpty());
    const auto request = transport.snapshots.takeFirst();
    Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                      snapshotWire(QStringLiteral("fixture")));
    QTRY_VERIFY(model.ready());

    QQuickView view;
    view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = Apps::SettingsAppearance::ensureTokenFacade(*view.engine(), &error);
    QVERIFY2(facade, qPrintable(error));
    const auto theme = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QVERIFY2(facade->publish(theme.theme, {}, &error), qPrintable(error));
    QVERIFY2(QindaQt::Shell::Icons::IconRuntime::install(
                 *view.engine(),
                 {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
                 {QStringLiteral("QindaQt")}),
             "icon runtime install");
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setInitialProperties({{QStringLiteral("customizeSettings"),
                               QVariant::fromValue(static_cast<QObject *>(&model))}});
    view.setSource(QUrl::fromLocalFile(QStringLiteral(QINDAQT_CUSTOMIZE_PAGE_QML_PATH)));
    QCOMPARE(view.status(), QQuickView::Ready);
    view.resize(width, 720);
    view.show();
    QTest::qWait(50);
    const auto initial = model.panels();
    auto *palette = findItem(view.rootObject(), QStringLiteral("customizePalette_clock"));
    auto *target = findItem(view.rootObject(), QStringLiteral("customizeDrop_dock_end"));
    QVERIFY(palette);
    QVERIFY(target);
    const auto from = center(palette);
    const auto to = center(target);
    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, from);
    movePointer(view, from, to);
    QTRY_VERIFY(model.visualDragActive());
    QVERIFY(model.dropAccepted());
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, to);
    QTRY_VERIFY(!model.visualDragActive());
    QVERIFY(model.dirty());
    QVERIFY(model.undo());
    QCOMPARE(model.panels(), initial);
    QVERIFY(!model.canUndo());

    auto *chip = findItem(view.rootObject(), QStringLiteral("customizeChip_clock-instance"));
    QVERIFY(chip);
    const auto chipFrom = center(chip);
    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, chipFrom);
    movePointer(view, chipFrom, to);
    QTRY_VERIFY(model.visualDragActive());
    QVERIFY(model.dropAccepted());
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, to);
    QTRY_VERIFY(!model.visualDragActive());
    QVERIFY(model.dirty());
    QVERIFY(model.undo());
    QCOMPARE(model.panels(), initial);

    palette = findItem(view.rootObject(), QStringLiteral("customizePalette_clock"));
    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, center(palette));
    movePointer(view, center(palette), to);
    QTRY_VERIFY(model.visualDragActive());
    QTest::keyClick(&view, Qt::Key_Escape);
    QTRY_VERIFY(!model.visualDragActive());
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, to);
    QCOMPARE(model.panels(), initial);
    QVERIFY(!model.dirty());

    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, from);
    movePointer(view, from, to);
    QTRY_VERIFY(model.visualDragActive());
    const QPoint outside(5, 5);
    movePointer(view, to, outside);
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, outside);
    QTRY_VERIFY(!model.visualDragActive());
    QCOMPARE(model.panels(), initial);
    QVERIFY(!model.dirty());
}

QTEST_MAIN(CustomizePointerTests)
#include "tst_customize_pointer.moc"
