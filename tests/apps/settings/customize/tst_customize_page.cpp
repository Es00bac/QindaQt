// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_customize_settings_model.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

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

// The canvas Image declares its QML fillMode through a metaobject enum
// (QQuickImage is private API); resolve symbolic values at runtime instead of
// hardcoding private integer constants.
int fillModeValue(QQuickItem *image, const char *key)
{
    const QMetaObject *meta = image->metaObject();
    const int index = meta->indexOfProperty("fillMode");
    if (index < 0) {
        return -1;
    }
    return meta->property(index).enumerator().keyToValue(key);
}

// Shared production-page load: token facade, the shipped icon theme (so both
// callers exercise resolved glyph rendering, not placeholders), and the
// compiled CustomizePage.qml bound to `model`.
bool loadCustomizePage(QQuickView &view, StubCustomizeSettingsModel &model,
                       QString *error)
{
    view.engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
        *view.engine(), error);
    if (facade == nullptr) {
        return false;
    }
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!theme.ok) {
        *error = theme.error;
        return false;
    }
    if (!facade->publish(theme.theme, {}, error)) {
        return false;
    }
    if (!QindaQt::Shell::Icons::IconRuntime::install(
            *view.engine(), {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
            {QStringLiteral("QindaQt")})) {
        *error = QStringLiteral("icon runtime install");
        return false;
    }
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setInitialProperties({{QStringLiteral("customizeSettings"),
                                QVariant::fromValue(static_cast<QObject *>(&model))}});
    view.setSource(QUrl::fromLocalFile(QStringLiteral(QINDAQT_CUSTOMIZE_PAGE_QML_PATH)));
    if (view.status() != QQuickView::Ready) {
        *error = QStringLiteral("view not ready");
        return false;
    }
    return view.rootObject() != nullptr;
}

} // namespace

class CustomizePageTests final : public QObject {
    Q_OBJECT

private slots:
    void rendersCompactAndWideWithoutLosingAccessibleEditors();
    void rendersAppletSettingEditorsInWideMode();
    void canvasFollowsConfiguredWallpaperAndFallsBackToTokens();
};

void CustomizePageTests::rendersCompactAndWideWithoutLosingAccessibleEditors()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString loadError;
    QVERIFY2(loadCustomizePage(view, model, &loadError), qPrintable(loadError));

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

    // The layout gallery is the route's primary switcher: every catalog
    // profile renders a visible miniature card with an accessible name.
    auto *profileCard = item(view.rootObject(),
                             "customizeProfileCard_fixture");
    QVERIFY(profileCard != nullptr);
    QVERIFY(profileCard->isVisible());
    QVERIFY2(accessibleName(profileCard)
                 .contains(QStringLiteral("layout profile"),
                           Qt::CaseInsensitive),
             qPrintable(accessibleName(profileCard)));

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

    auto *primaryScope = item(view.rootObject(), "customizeDisplayScopePrimary");
    auto *allScope = item(view.rootObject(), "customizeDisplayScopeAll");
    auto *scopeError = item(view.rootObject(), "customizeDisplayScopeError");
    QVERIFY(primaryScope != nullptr);
    QVERIFY(allScope != nullptr);
    QVERIFY(scopeError != nullptr);
    QVERIFY(primaryScope->isVisible());
    QVERIFY(allScope->isVisible());
    QVERIFY2(accessibleName(primaryScope).contains(QStringLiteral("Primary display")),
             qPrintable(accessibleName(primaryScope)));
    QVERIFY2(accessibleName(allScope).contains(QStringLiteral("All displays")),
             qPrintable(accessibleName(allScope)));
    allScope->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(allScope->hasActiveFocus());
    QTest::keyClick(&view, Qt::Key_Space);
    QTRY_COMPARE(model.configurePanelCalls, 1);
    QCOMPARE(model.lastConfiguredField, QStringLiteral("outputScope"));
    QCOMPARE(model.lastConfiguredValue, QVariant(QStringLiteral("all")));

    primaryScope->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(primaryScope->hasActiveFocus());
    QTest::keyClick(&view, Qt::Key_Space);
    QTRY_COMPARE(model.configurePanelCalls, 2);
    QCOMPARE(model.lastConfiguredField, QStringLiteral("outputScope"));
    QCOMPARE(model.lastConfiguredValue, QVariant(QStringLiteral("primary")));

    model.setPrimaryDisplayAvailable(false);
    QTRY_VERIFY(!primaryScope->isEnabled());
    QTRY_VERIFY(scopeError->isVisible());
    QVERIFY2(accessibleName(scopeError).contains(QStringLiteral("not currently known")),
             qPrintable(accessibleName(scopeError)));

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

void CustomizePageTests::rendersAppletSettingEditorsInWideMode()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString loadError;
    QVERIFY2(loadCustomizePage(view, model, &loadError), qPrintable(loadError));
    view.resize(1080, 720);
    view.show();
    QTest::qWait(50);

    model.selectApplet(QStringLiteral("bar"), QStringLiteral("clock-instance"));
    auto *settingSwitch = item(view.rootObject(), "customizeAppletSettingSwitch_showIcon");
    auto *settingSlider = item(view.rootObject(), "customizeAppletSettingSlider_refreshSeconds");
    auto *settingChoice = item(view.rootObject(), "customizeAppletSettingChoice_alignment");
    auto *readOnlyRow = item(view.rootObject(), "customizeAppletSettingError");
    QVERIFY(settingSwitch != nullptr);
    QVERIFY(settingSlider != nullptr);
    QVERIFY(settingChoice != nullptr);
    QVERIFY(readOnlyRow != nullptr);
    QVERIFY(settingSwitch->isVisible());
    QVERIFY(settingSlider->isVisible());
    QVERIFY(settingChoice->isVisible());
    QVERIFY2(accessibleName(settingSwitch).contains(QStringLiteral("showIcon")),
             qPrintable(accessibleName(settingSwitch)));

    settingSwitch->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(settingSwitch->hasActiveFocus());
    QTest::keyClick(&view, Qt::Key_Space);
    QTRY_COMPARE(model.configureAppletSettingCalls, 1);
    QCOMPARE(model.lastConfiguredAppletKey, QStringLiteral("showIcon"));
    QCOMPARE(model.lastConfiguredAppletValue, QVariant(false));

    settingSlider->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(settingSlider->hasActiveFocus());
    QTest::keyClick(&view, Qt::Key_Right);
    QTRY_COMPARE(model.configureAppletSettingCalls, 2);
    QCOMPARE(model.lastConfiguredAppletKey, QStringLiteral("refreshSeconds"));

    // The declared-but-Unsupported freeform string field (labelFormat) stays
    // the quiet read-only row rather than gaining an invented free-text
    // editor: its sibling editor controls exist (one delegate instantiates
    // all four candidate rows) but none of them is visible.
    auto *labelFormatSwitch =
        item(view.rootObject(), "customizeAppletSettingSwitch_labelFormat");
    auto *labelFormatChoice =
        item(view.rootObject(), "customizeAppletSettingChoice_labelFormat");
    auto *labelFormatSlider =
        item(view.rootObject(), "customizeAppletSettingSlider_labelFormat");
    QVERIFY(labelFormatSwitch == nullptr || !labelFormatSwitch->isVisible());
    QVERIFY(labelFormatChoice == nullptr || !labelFormatChoice->isVisible());
    QVERIFY(labelFormatSlider == nullptr || !labelFormatSlider->isVisible());

    model.setAppletSettingError(QStringLiteral("'refreshSeconds' must be between 1 and 60"));
    QTRY_VERIFY(readOnlyRow->isVisible());
    QVERIFY2(accessibleName(readOnlyRow).contains(QStringLiteral("between 1 and 60")),
             qPrintable(accessibleName(readOnlyRow)));
    model.setAppletSettingError(QString());
}

void CustomizePageTests::canvasFollowsConfiguredWallpaperAndFallsBackToTokens()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString loadError;
    QVERIFY2(loadCustomizePage(view, model, &loadError), qPrintable(loadError));
    view.resize(1080, 720);
    view.show();
    QTest::qWait(50);

    // Red-before contract: the canvas must surface a wallpaper item driven by
    // the configured wallpaper truth, not only the decorative gradient.
    auto *wallpaper = item(view.rootObject(), "customizeCanvasWallpaper");
    QVERIFY(wallpaper != nullptr);

    // No configured wallpaper (explicit "none" truth) keeps the token
    // gradient: the image stays hidden and loads nothing.
    QVERIFY(!wallpaper->isVisible());
    QCOMPARE(wallpaper->property("source").toUrl(), QUrl());

    const QUrl bundled = QUrl::fromLocalFile(QStringLiteral(
        QINDAQT_SOURCE_DIR "/data/wallpapers/jade-fold.png"));
    model.wallpaperPreviewFixture()->configure(bundled, QStringLiteral("tiled"));
    QTRY_VERIFY(wallpaper->isVisible());
    QCOMPARE(wallpaper->property("source").toUrl(), bundled);
    QCOMPARE(wallpaper->property("fillMode").toInt(),
             fillModeValue(wallpaper, "Tile"));

    model.wallpaperPreviewFixture()->configure(bundled, QStringLiteral("centered"));
    QTRY_COMPARE(wallpaper->property("fillMode").toInt(),
                 fillModeValue(wallpaper, "Pad"));

    model.wallpaperPreviewFixture()->configure(bundled, QStringLiteral("scaled"));
    QTRY_COMPARE(wallpaper->property("fillMode").toInt(),
                 fillModeValue(wallpaper, "PreserveAspectCrop"));

    // Unavailable/invalid Settings1 truth fails closed back to the gradient.
    model.wallpaperPreviewFixture()->configure(QUrl(), QStringLiteral("scaled"),
                                               QStringLiteral("unavailable"));
    QTRY_VERIFY(!wallpaper->isVisible());
    QCOMPARE(wallpaper->property("source").toUrl(), QUrl());
}

QTEST_MAIN(CustomizePageTests)
#include "tst_customize_page.moc"
