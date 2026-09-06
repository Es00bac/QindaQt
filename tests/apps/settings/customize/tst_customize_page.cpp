// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_customize_settings_model.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

using QindaQt::Apps::SettingsCustomize::TestSupport::StubCustomizeSettingsModel;

namespace {

QQuickItem *item(QObject *root, const char *name)
{
    auto *rootItem = qobject_cast<QQuickItem *>(root);
    if (rootItem == nullptr) {
        return nullptr;
    }
    if (rootItem->objectName() == QLatin1String(name)) {
        return rootItem;
    }
    for (QQuickItem *child : rootItem->childItems()) {
        if (auto *match = item(child, name); match != nullptr) {
            return match;
        }
    }
    return nullptr;
}

QString accessibleName(QQuickItem *candidate)
{
    if (candidate == nullptr) {
        return {};
    }
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(candidate);
    return interface == nullptr ? QString{}
                                : interface->text(QAccessible::Name);
}

} // namespace

class CustomizePageTests final : public QObject {
    Q_OBJECT

private slots:
    void rendersCompactAndWideWithoutLosingAccessibleEditors();
};

void CustomizePageTests::rendersCompactAndWideWithoutLosingAccessibleEditors()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString facadeError;
    auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
        *view.engine(), &facadeError);
    QVERIFY2(facade != nullptr, qPrintable(facadeError));
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QString publishError;
    QVERIFY2(facade->publish(theme.theme, {}, &publishError),
             qPrintable(publishError));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setInitialProperties({{QStringLiteral("customizeSettings"),
                                QVariant::fromValue(static_cast<QObject *>(&model))}});
    view.setSource(QUrl::fromLocalFile(QStringLiteral(QINDAQT_CUSTOMIZE_PAGE_QML_PATH)));
    QCOMPARE(view.status(), QQuickView::Ready);
    QVERIFY(view.rootObject() != nullptr);

    view.resize(720, 720);
    view.show();
    QTest::qWait(50);
    auto *compact = item(view.rootObject(), "customizeCompactLayout");
    auto *wide = item(view.rootObject(), "customizeWideLayout");
    QVERIFY(compact != nullptr);
    QVERIFY(wide != nullptr);
    QVERIFY(compact->isVisible());
    QVERIFY(!wide->isVisible());
    QVERIFY(item(view.rootObject(), "customizeOutputCanvas") != nullptr);

    auto *paletteButton = item(compact, "customizePalette_clock");
    auto *panelButton = item(compact, "customizeOutlinePanel_bar");
    auto *zoneButton = item(compact, "customizeOutlineZone_bar_end");
    QVERIFY2(accessibleName(paletteButton).contains(QStringLiteral("clock applet"),
                                                    Qt::CaseInsensitive),
             qPrintable(accessibleName(paletteButton)));
    QVERIFY2(accessibleName(panelButton).contains(QStringLiteral("panel"),
                                                  Qt::CaseInsensitive),
             qPrintable(accessibleName(panelButton)));
    QVERIFY2(accessibleName(zoneButton).contains(QStringLiteral("end zone"),
                                                 Qt::CaseInsensitive),
             qPrintable(accessibleName(zoneButton)));

    QVERIFY(paletteButton != nullptr);
    paletteButton->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(paletteButton->hasActiveFocus());
    QTest::keyClick(&view, Qt::Key_Space);
    QTRY_COMPARE(model.keyboardInsertCalls, 1);

    view.resize(1080, 720);
    QTest::qWait(50);
    QVERIFY(!compact->isVisible());
    QVERIFY(wide->isVisible());
    QVERIFY(item(view.rootObject(), "customizeThicknessSlider") != nullptr);

    model.setDirty(true);
    QVERIFY(QMetaObject::invokeMethod(view.rootObject(), "requestClose"));
    auto *discardDialog = view.rootObject()->findChild<QObject *>(
        QStringLiteral("customizeDiscardDialog"));
    QVERIFY(discardDialog != nullptr);
    QTRY_VERIFY(discardDialog->property("visible").toBool());
    const qreal expectedX = (view.width()
                             - discardDialog->property("width").toReal()) / 2.0;
    const qreal expectedY = (view.height()
                             - discardDialog->property("height").toReal()) / 2.0;
    QVERIFY(qAbs(discardDialog->property("x").toReal() - expectedX) < 1.0);
    QVERIFY(qAbs(discardDialog->property("y").toReal() - expectedY) < 1.0);
    QVERIFY(QMetaObject::invokeMethod(discardDialog, "reject"));

    // A 960px Settings window gives the route a medium-width work area after
    // the Settings Center sidebar. Its primary Arrange task must therefore
    // keep the representative desktop and all scaled applet markers contained.
    view.resize(960, 680);
    QTest::qWait(50);
    QVERIFY(compact->isVisible());
    auto *canvas = item(view.rootObject(), "customizeOutputCanvas");
    auto *panel = item(view.rootObject(), "customizeCanvasPanel_bar");
    auto *chip = item(view.rootObject(), "customizeChip_clock-instance");
    QVERIFY(canvas != nullptr);
    QVERIFY(panel != nullptr);
    QVERIFY(chip != nullptr);
    const QRectF chipBounds = chip->mapRectToItem(panel, chip->boundingRect());
    QVERIFY(chipBounds.left() >= 0.0);
    QVERIFY(chipBounds.top() >= 0.0);
    QVERIFY(chipBounds.right() <= panel->width());
    QVERIFY(chipBounds.bottom() <= panel->height());

    auto *outlineTab = item(view.rootObject(), "customizeCompactTab_1");
    auto *detailsTab = item(view.rootObject(), "customizeCompactTab_2");
    QVERIFY(outlineTab != nullptr);
    QVERIFY(detailsTab != nullptr);
    QVERIFY(QMetaObject::invokeMethod(outlineTab, "click"));
    QTRY_COMPARE(view.rootObject()->property("compactSection").toInt(), 1);
    QTRY_VERIFY(item(compact, "customizeOutlinePanel_bar")->isVisible());
    QVERIFY(QMetaObject::invokeMethod(detailsTab, "click"));
    QTRY_COMPARE(view.rootObject()->property("compactSection").toInt(), 2);
    QTRY_VERIFY(item(compact, "customizeProperties")->isVisible());
}

QTEST_MAIN(CustomizePageTests)
#include "tst_customize_page.moc"
