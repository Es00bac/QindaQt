// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_appearance_model.h"
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QFont>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>
#include <QUrl>

#include <functional>
#include <memory>

using QindaQt::Apps::SettingsAppearance::ensureTokenFacade;

namespace {

const char *const BuildQmlImportPath = QINDAQT_QML_IMPORT_PATH;
const char *const SceneQmlDir = QINDAQT_APPEARANCE_TEST_QML_DIR;

QVariantMap themeEntry(const QString &id, const QString &name,
                       const QString &variant,
                       const QVariantMap &previewTokens)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("name"), name},
            {QStringLiteral("variant"), variant},
            {QStringLiteral("previewTokens"), previewTokens}};
}

QVariantMap previewTokensFor(const QString &themeFile)
{
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + themeFile);
    if (!loaded.ok) {
        return {};
    }
    const auto derived = QindaQt::DesignTokens::DesignTokenDeriver::derive(
        loaded.theme, {});
    return derived.ok() ? derived.tokens->toVariantMap() : QVariantMap{};
}

QVariantMap defaultDraftMap()
{
    return {{QStringLiteral("appearance.theme"),
             QStringLiteral("qinda-dark")},
            {QStringLiteral("appearance.colorScheme"),
             QStringLiteral("system")},
            {QStringLiteral("fonts.family"), QStringLiteral("Noto Sans")},
            {QStringLiteral("fonts.pointSize"), 10.0},
            {QStringLiteral("fonts.antialiasing"), true},
            {QStringLiteral("fonts.hinting"), QStringLiteral("slight")},
            {QStringLiteral("fonts.subpixelOrder"), QStringLiteral("rgb")},
            {QStringLiteral("appearance.wallpaper"), QString()},
            {QStringLiteral("appearance.wallpaperMode"),
             QStringLiteral("scaled")},
            {QStringLiteral("appearance.uiScale"), 1.0}};
}

// AGENT-CONTRACT: Members are declared model-first so reverse destruction
// tears the view (and its QML bindings) down before the stub model dies.
struct Scene final {
    std::unique_ptr<StubAppearanceModel> model;
    std::unique_ptr<QQuickView> view;
    QQuickItem *root = nullptr;
    QString error;
};

Scene createScene(const std::function<void(StubAppearanceModel &)> &configure)
{
    Scene scene;
    scene.model = std::make_unique<StubAppearanceModel>();
    if (configure) {
        configure(*scene.model);
    }
    scene.view = std::make_unique<QQuickView>();
    scene.view->engine()->addImportPath(QString::fromUtf8(BuildQmlImportPath));
    QString error;
    auto *facade = ensureTokenFacade(*scene.view->engine(), &error);
    // The destination tabs render shipped glyphs through the confined icon
    // provider; the harness installs it so the strip proves resolved
    // iconography, not provider warnings.
    if (!QindaQt::Shell::Icons::IconRuntime::install(
            *scene.view->engine(), {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
            {QStringLiteral("QindaQt")})) {
        scene.error = QStringLiteral("icon runtime install failed");
        return scene;
    }
    if (facade == nullptr) {
        scene.error = QStringLiteral("could not bind test tokens: %1").arg(error);
        return scene;
    }
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!theme.ok || !facade->publish(theme.theme, {})) {
        scene.error = QStringLiteral("could not publish test tokens");
        return scene;
    }
    scene.view->setResizeMode(QQuickView::SizeRootObjectToView);
    scene.view->resize(640, 640);
    scene.view->setInitialProperties(
        {{QStringLiteral("stubModel"), QVariant::fromValue(scene.model.get())}});
    scene.view->setSource(QUrl::fromLocalFile(
        QString::fromLatin1(SceneQmlDir)
        + QStringLiteral("/AppearancePageScene.qml")));
    if (!scene.view->errors().isEmpty()) {
        scene.error = QStringLiteral("could not load appearance scene: %1")
                          .arg(scene.view->errors().constFirst().toString());
        return scene;
    }
    scene.view->show();
    QCoreApplication::processEvents();
    scene.root = scene.view->rootObject();
    return scene;
}

QQuickItem *item(const QQuickItem *root, const QString &wanted)
{
    if (root->objectName() == wanted) {
        return const_cast<QQuickItem *>(root);
    }
    // Repeater delegates are visual children but are not guaranteed to use
    // the containing item as their QObject parent. Traverse the scene graph,
    // which is the ownership relation this presentation test exercises.
    for (QQuickItem *child : root->childItems()) {
        if (auto *match = item(child, wanted); match != nullptr) {
            return match;
        }
    }
    return nullptr;
}

QString descendantObjectNames(const QObject *root)
{
    QStringList names;
    for (const QObject *object : root->findChildren<QObject *>()) {
        if (!object->objectName().isEmpty()) {
            names.append(object->objectName());
        }
    }
    names.sort();
    return names.join(QStringLiteral(", "));
}

QAccessible::Role roleOf(QQuickItem *item_)
{
    auto *interface = QAccessible::queryAccessibleInterface(item_);
    if (interface == nullptr) {
        qWarning("missing accessible interface for %s",
                 qPrintable(item_->objectName()));
        return QAccessible::NoRole;
    }
    return interface->role();
}

} // namespace

class AppearancePageTests final : public QObject {
    Q_OBJECT

private slots:
    void themeCardsRenderSelectAndGate();
    void toggleHandlersForwardAuthoritativeCheckedValues();
    void textEditorsForwardOrdinaryUserInput();
    void actionRowWiresApplyRevertRetryClose();
    void statusFallbackAndAccessibilityTruth();
    void saveResultSummaryIsAccessibleAndTruthful();
    void focusedDestinationNavigationKeepsDraftAndControlsReachable();
    void windowsDestinationPreviewsBothChromeSetsAndForwardsChoices_data();
    void windowsDestinationPreviewsBothChromeSetsAndForwardsChoices();
    void qtToolkitCardReflectsThePlatformThemeProjection();
    void fontTypingWallpaperPreviewAndKeyboardScrollingStayUsable();

private:
    static void makeReady(StubAppearanceModel &model, bool dirty)
    {
        model.loading = false;
        model.ready = true;
        model.canEdit = true;
        model.statusText.clear();
        model.draftDirty = dirty;
        model.draftValid = true;
        model.applyAvailable = dirty;
    }
};

QQuickItem *activateDestination(const Scene &scene, const QString &destination)
{
    auto *button = item(scene.root,
                        "appearanceDestination_" + destination);
    if (button == nullptr) {
        button = item(scene.root,
                      "appearanceCompactDestination_" + destination);
    }
    if (button == nullptr) {
        return nullptr;
    }
    if (!QMetaObject::invokeMethod(button, "click")) {
        return nullptr;
    }
    QCoreApplication::processEvents();
    return item(scene.root, "appearanceDestinationPage_" + destination);
}

void AppearancePageTests::themeCardsRenderSelectAndGate()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{
            themeEntry(QStringLiteral("qinda-dark"),
                       QStringLiteral("Qinda Dark"), QStringLiteral("dark"),
                       previewTokensFor(QStringLiteral("qinda-dark.json"))),
            themeEntry(QStringLiteral("qinda-light"),
                       QStringLiteral("Qinda Light"), QStringLiteral("light"),
                       previewTokensFor(QStringLiteral("qinda-light.json")))};
        model.draft = defaultDraftMap();
        makeReady(model, false);
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    QCOMPARE(scene.model->property("installedThemes").toList().size(), 2);
    auto *repeater = scene.root->findChild<QObject *>(
        QStringLiteral("appearanceThemeRepeater"));
    QVERIFY(repeater != nullptr);
    QCOMPARE(repeater->property("count").toInt(), 2);

    QQuickItem *darkCard = nullptr;
    QQuickItem *lightCard = nullptr;
    QVERIFY2(QTest::qWaitFor(
                 [&]() {
                     darkCard = item(scene.root,
                                     "appearanceThemeCard_qinda-dark");
                     lightCard = item(scene.root,
                                      "appearanceThemeCard_qinda-light");
                     return darkCard != nullptr && lightCard != nullptr;
                 },
                 1'000),
             qPrintable(QStringLiteral("theme cards missing; descendants: %1")
                            .arg(descendantObjectNames(scene.root))));
    QVERIFY(darkCard->isEnabled());
    QVERIFY(darkCard->property("checked").toBool());
    QVERIFY(!lightCard->property("checked").toBool());

    // Selecting a theme and its matching scheme forms one user draft.
    QVERIFY(QMetaObject::invokeMethod(lightCard, "click"));
    QTRY_COMPARE(scene.model->draftKeys.size(), 2);
    QCOMPARE(scene.model->draftKeys.constFirst(),
             QStringLiteral("appearance.theme"));
    QCOMPARE(scene.model->draftValues.constFirst().toString(),
             QStringLiteral("qinda-light"));
    QCOMPARE(scene.model->draftKeys.constLast(),
             QStringLiteral("appearance.colorScheme"));
    QCOMPARE(scene.model->draftValues.constLast().toString(),
             QStringLiteral("light"));

    // While saving, the fail-closed gate disables every theme card.
    scene.model->saving = true;
    scene.model->canEdit = false;
    scene.model->publish();
    QVERIFY(!darkCard->isEnabled());
    QVERIFY(!lightCard->isEnabled());
}

void AppearancePageTests::toggleHandlersForwardAuthoritativeCheckedValues()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{};
        model.draft = defaultDraftMap();
        makeReady(model, false);
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));

    QVERIFY(activateDestination(scene, QStringLiteral("fonts")) != nullptr);
    auto *antialiasing = item(scene.root, "appearanceAntialiasingSwitch");
    QVERIFY(antialiasing != nullptr);

    // QQuickAbstractButton::toggled() carries no Boolean argument. The QML
    // handlers must read each control's checked property after an ordinary
    // click instead of treating an absent signal parameter as false.
    QVERIFY(QMetaObject::invokeMethod(antialiasing, "click"));
    QTRY_COMPARE(scene.model->draftKeys.size(), 1);
    QCOMPARE(scene.model->draftKeys.constLast(),
             QStringLiteral("fonts.antialiasing"));
    QCOMPARE(scene.model->draftValues.constLast().toBool(), false);

    QVERIFY(activateDestination(scene, QStringLiteral("themes")) != nullptr);
    QQuickItem *darkScheme = nullptr;
    QTRY_VERIFY((darkScheme = item(scene.root, "appearanceSchemeButton_dark")) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(darkScheme, "click"));
    QTRY_COMPARE(scene.model->draftKeys.size(), 2);
    QCOMPARE(scene.model->draftKeys.constLast(),
             QStringLiteral("appearance.colorScheme"));
    QCOMPARE(scene.model->draftValues.constLast().toString(),
             QStringLiteral("dark"));
}

void AppearancePageTests::qtToolkitCardReflectsThePlatformThemeProjection()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.previewQtPalette = QVariantList{
            QVariantMap{{QStringLiteral("role"), QStringLiteral("Window")},
                        {QStringLiteral("color"), QStringLiteral("#211d27")}},
            QVariantMap{{QStringLiteral("role"), QStringLiteral("Selection")},
                        {QStringLiteral("color"), QStringLiteral("#eab391")}},
        };
    });
    QVERIFY(activateDestination(scene, QStringLiteral("themes")) != nullptr);
    QQuickItem *card = nullptr;
    QTRY_VERIFY((card = item(scene.root, "appearanceQtToolkitCard")) != nullptr);
    QVERIFY(card->isVisible());
    const auto swatches = card->findChildren<QQuickItem *>(
        QStringLiteral("appearanceQtPaletteSwatches"));
    QVERIFY(!swatches.isEmpty());
    // One swatch per projected role; the Repeater that instantiates them is
    // itself a child item of the Flow and must not be counted.
    int swatchCount = 0;
    for (const QQuickItem *child : swatches.first()->childItems()) {
        if (!child->inherits("QQuickRepeater")) {
            ++swatchCount;
        }
    }
    QCOMPARE(swatchCount, 2);
}

void AppearancePageTests::textEditorsForwardOrdinaryUserInput()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{};
        model.draft = defaultDraftMap();
        makeReady(model, false);
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));

    QVERIFY(activateDestination(scene, QStringLiteral("fonts")) != nullptr);
    auto *fontFamily = item(scene.root, "appearanceFontFamilyField");
    auto *subpixel = item(scene.root, "appearanceSubpixelSelector");
    QVERIFY(fontFamily != nullptr);
    QVERIFY(subpixel != nullptr);

    fontFamily->forceActiveFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(scene.view->activeFocusItem() != nullptr);
    // Real keystrokes replace the selected installed starting family; no Enter commits
    // this value. Moving focus to another selector must retain the draft.
    for (const auto key : {Qt::Key_Space, Qt::Key_S, Qt::Key_E, Qt::Key_R,
                           Qt::Key_I, Qt::Key_F})
        QTest::keyClick(scene.view.get(), key);
    subpixel->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(scene.view->activeFocusItem(), subpixel);
    QTRY_COMPARE(scene.model->draftKeys.constLast(), QStringLiteral("fonts.family"));
    QCOMPARE(scene.model->draftValues.constLast().toString(),
             QStringLiteral("Serif"));

    scene.model->draftDirty = true;
    scene.model->publish();
    auto *revert = item(scene.root, "appearanceRevertButton");
    QVERIFY(revert != nullptr && revert->isVisible());
    QCOMPARE(revert->property("text").toString(), QStringLiteral("Revert"));
    QVERIFY(QMetaObject::invokeMethod(revert, "click"));
    QTRY_COMPARE(scene.model->cancels, 1);

    // The real route publishes its confirmed draft after cancel succeeds. The
    // presentation fixture mirrors that publication and proves the editable
    // field releases its stale typed text rather than retaining it locally.
    scene.model->draft = defaultDraftMap();
    scene.model->draftDirty = false;
    scene.model->publish();
    QTRY_COMPARE(fontFamily->property("editText").toString(),
                 QStringLiteral("Noto Sans"));
    auto *fontText = qobject_cast<QObject *>(
        fontFamily->property("contentItem").value<QObject *>());
    QVERIFY(fontText != nullptr);
    QTRY_COMPARE(fontText->property("text").toString(),
                 QStringLiteral("Noto Sans"));

    QVERIFY(activateDestination(scene, QStringLiteral("wallpaper")) != nullptr);
    auto *wallpaper = item(scene.root, "appearanceWallpaperField");
    QVERIFY(wallpaper != nullptr);
    wallpaper->forceActiveFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(wallpaper->hasActiveFocus());
    QTest::keyClick(scene.view.get(), Qt::Key_Slash);
    QTRY_COMPARE(scene.model->draftKeys.constLast(),
                 QStringLiteral("appearance.wallpaper"));
    QCOMPARE(scene.model->draftValues.constLast().toString(), QStringLiteral("/"));
}

void AppearancePageTests::fontTypingWallpaperPreviewAndKeyboardScrollingStayUsable()
{
    const QUrl bundledPreview = QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/wallpapers/qinda-punk.png"));
    const auto scene = createScene([&](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{};
        model.bundledWallpapers = QVariantList{QVariantMap{
            {QStringLiteral("name"), QStringLiteral("Qinda Punk")},
            {QStringLiteral("value"), QStringLiteral("qindaqt:qinda-punk")},
            {QStringLiteral("previewUrl"), bundledPreview},
        }};
        model.draft = defaultDraftMap();
        makeReady(model, false);
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));

    QVERIFY(activateDestination(scene, QStringLiteral("wallpaper")) != nullptr);
    auto *bundled = item(scene.root, "bundledWallpaperButton");
    auto *preview = item(scene.root, "appearanceWallpaperPreview");
    QVERIFY(bundled != nullptr && preview != nullptr);
    QVERIFY(QMetaObject::invokeMethod(bundled, "click"));
    QTRY_COMPARE(scene.model->draftValues.constLast().toString(),
                 QStringLiteral("qindaqt:qinda-punk"));
    QTRY_COMPARE(preview->property("source").toUrl(), bundledPreview);

    scene.view->resize(640, 320);
    QVERIFY(activateDestination(scene, QStringLiteral("fonts")) != nullptr);
    auto *appearancePage = item(scene.root, "appearancePage");
    auto *viewport = item(scene.root, "appearanceFormViewport");
    auto *subpixel = item(scene.root, "appearanceSubpixelSelector");
    QVERIFY(appearancePage != nullptr && viewport != nullptr && subpixel != nullptr);
    appearancePage->forceActiveFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(appearancePage->hasActiveFocus());
    QTest::keyClick(scene.view.get(), Qt::Key_PageDown);
    QTRY_VERIFY(viewport->property("contentY").toReal() > 0.0);
    appearancePage->forceActiveFocus(Qt::OtherFocusReason);
    QTRY_VERIFY(appearancePage->hasActiveFocus());
    subpixel->forceActiveFocus(Qt::TabFocusReason);
    QTRY_COMPARE(scene.view->activeFocusItem(), subpixel);
    QTRY_VERIFY(viewport->property("contentY").toReal() > 0.0);
}

void AppearancePageTests::actionRowWiresApplyRevertRetryClose()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{};
        model.draft = defaultDraftMap();
        makeReady(model, true);
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    QCOMPARE(scene.model->property("installedThemes").toList().size(), 0);

    auto *apply = item(scene.root, "appearanceApplyButton");
    auto *revert = item(scene.root, "appearanceRevertButton");
    auto *retry = item(scene.root, "appearanceRetryButton");
    QVERIFY(apply != nullptr && revert != nullptr && retry != nullptr);
    QVERIFY(apply->isVisible());
    QVERIFY(revert->isVisible());
    QVERIFY(!retry->isVisible());

    QMetaObject::invokeMethod(apply, "clicked");
    QCOMPARE(scene.model->applies, 1);
    QMetaObject::invokeMethod(revert, "clicked");
    QCOMPARE(scene.model->cancels, 1);

    // Unavailable truth swaps Apply for Retry; Retry is wired without resubmit.
    scene.model->unavailable = true;
    scene.model->ready = false;
    scene.model->canEdit = false;
    scene.model->applyAvailable = false;
    scene.model->draftDirty = false;
    scene.model->saving = false;
    scene.model->statusText =
        QStringLiteral("Last confirmed appearance settings retained; refresh to continue");
    scene.model->publish();
    QTRY_VERIFY(retry->isVisible());
    QMetaObject::invokeMethod(retry, "clicked");
    QCOMPARE(scene.model->retries, 1);
    QCOMPARE(scene.model->applies, 1);
}

void AppearancePageTests::statusFallbackAndAccessibilityTruth()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{};
        model.draft = defaultDraftMap();
        model.conflict = true;
        model.statusText =
            QStringLiteral("Appearance changed elsewhere; current values reloaded");
        model.errorText = QStringLiteral("durable save failed");
        model.configuredThemeInstalled = false;
        model.fallbackNotice = QStringLiteral(
            "Configured theme 'ghost' is not installed; previewing 'qinda-dark'");
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));

    auto *status = item(scene.root, "appearanceStatus");
    auto *error = item(scene.root, "appearanceError");
    auto *fallback = item(scene.root, "appearanceThemeFallback");
    QVERIFY(status != nullptr && error != nullptr && fallback != nullptr);
    QCOMPARE(roleOf(status), QAccessible::AlertMessage);
    QCOMPARE(roleOf(error), QAccessible::AlertMessage);
    QCOMPARE(status->property("text").toString(),
             QStringLiteral("Appearance changed elsewhere; current values reloaded"));
    QCOMPARE(error->property("text").toString(),
             QStringLiteral("durable save failed"));
    QVERIFY(fallback->isVisible());
    QCOMPARE(fallback->property("text").toString(),
             QStringLiteral("Configured theme 'ghost' is not installed; previewing 'qinda-dark'"));
}

void AppearancePageTests::saveResultSummaryIsAccessibleAndTruthful()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{};
        model.draft = defaultDraftMap();
        makeReady(model, true);
        model.saveResultsHaveFailure = true;
        model.saveResultsText = QStringLiteral(
            "Save results: appearance.theme — Applied; fonts.pointSize — Failed: disk full");
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    auto *summary = item(scene.root, "appearanceSaveResults");
    QVERIFY(summary != nullptr);
    QVERIFY(summary->isVisible());
    QCOMPARE(roleOf(summary), QAccessible::AlertMessage);
    QVERIFY(summary->property("text").toString().contains(
        QStringLiteral("appearance.theme — Applied")));
    QVERIFY(summary->property("text").toString().contains(
        QStringLiteral("fonts.pointSize — Failed")));
}

void AppearancePageTests::focusedDestinationNavigationKeepsDraftAndControlsReachable()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{themeEntry(
            QStringLiteral("qinda-dark"), QStringLiteral("Qinda Dark"),
            QStringLiteral("dark"),
            previewTokensFor(QStringLiteral("qinda-dark.json")))};
        model.draft = defaultDraftMap();
        makeReady(model, true);
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    auto *appearancePage = item(scene.root, "appearancePage");
    QVERIFY(appearancePage != nullptr);

    QVERIFY(activateDestination(scene, QStringLiteral("themes")) != nullptr);
    QVERIFY(item(scene.root, "appearanceThemeCard_qinda-dark") != nullptr);
    QCOMPARE(appearancePage->property("currentDestination").toString(),
             QStringLiteral("themes"));

    QVERIFY(activateDestination(scene, QStringLiteral("wallpaper")) != nullptr);
    QVERIFY(item(scene.root, "appearanceWallpaperField") != nullptr);
    QCOMPARE(appearancePage->property("currentDestination").toString(),
             QStringLiteral("wallpaper"));

    QVERIFY(activateDestination(scene, QStringLiteral("fonts")) != nullptr);
    auto *fontFamily = item(scene.root, "appearanceFontFamilyField");
    auto *subpixel = item(scene.root, "appearanceSubpixelSelector");
    QVERIFY(fontFamily != nullptr);
    QVERIFY(subpixel != nullptr);
    // The shared token selector exposes this presentation contract. It keeps
    // both closed fields out of the platform's bright native ComboBox style.
    QVERIFY(fontFamily->property("transitionDuration").isValid());
    QVERIFY(subpixel->property("transitionDuration").isValid());
    QVERIFY(fontFamily->implicitHeight() >= 40.0);
    QVERIFY(subpixel->implicitHeight() >= 40.0);
    QVERIFY(item(scene.root, "appearanceAntialiasingSwitch") != nullptr);

    // Destination changes only choose presentation. The page keeps the one
    // route-level draft/action boundary for every preference category.
    auto *summary = item(scene.root, "appearanceDraftSummary");
    auto *apply = item(scene.root, "appearanceApplyButton");
    QVERIFY(summary != nullptr && apply != nullptr);
    QVERIFY(item(scene.root, "appearanceCompactDestinationList") == nullptr);
    QVERIFY(summary->property("wrapMode").isValid());
    QVERIFY(summary->property("text").toString().contains(
        QStringLiteral("Changes have not been applied")));
    QVERIFY(apply->isVisible());
}

void AppearancePageTests::windowsDestinationPreviewsBothChromeSetsAndForwardsChoices_data()
{
    QTest::addColumn<QString>("themeId");
    QTest::newRow("unauthored-decoration") << QStringLiteral("qinda-dusk");
    QTest::newRow("authored-decoration") << QStringLiteral("qinda-bliss");
}

void AppearancePageTests::windowsDestinationPreviewsBothChromeSetsAndForwardsChoices()
{
    QFETCH(QString, themeId);
    const auto loaded = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/") + themeId + QStringLiteral(".json"));
    QVERIFY2(loaded.ok, qPrintable(loaded.error));
    const auto theme = loaded.theme;
    const auto scene = createScene([&theme](StubAppearanceModel &model) {
        model.draft = defaultDraftMap();
        publishResolvedChrome(model, theme);
        makeReady(model, false);
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));

    // ADR-0129: one destination shows both chrome sets, each through its
    // real renderer, and forwards arrangement choices to the one draft.
    QVERIFY(activateDestination(scene, QStringLiteral("windows")) != nullptr);
    QQuickItem *windowPreview = nullptr;
    QTRY_VERIFY((windowPreview = item(scene.root, "appearanceWindowChromePreview")) != nullptr);
    auto *containerPreview = item(scene.root, "appearanceContainerChromePreview");
    QVERIFY(containerPreview != nullptr);
    QTRY_VERIFY(containerPreview->width() > 0.0);
    bool builds = false;
    QVERIFY(QMetaObject::invokeMethod(containerPreview, "layoutBuilds",
                                      Q_RETURN_ARG(bool, builds)));
    QVERIFY2(builds, "the compositor layout engine rejected the container preview");

    const QByteArray captureDirectory = qgetenv("QINDAQT_APPEARANCE_CAPTURE_DIR");
    if (!captureDirectory.isEmpty()) {
        // Design-review hook: render the destination tall enough to hold both
        // previews and save it for visual inspection.
        scene.view->resize(900, 1500);
        QTest::qWait(300);
        const QImage frame = scene.view->grabWindow();
        QVERIFY(frame.save(QString::fromLocal8Bit(captureDirectory) + QStringLiteral("/windows-")
                           + themeId + QStringLiteral(".png")));
    }

    QQuickItem *rightSide = nullptr;
    QTRY_VERIFY((rightSide = item(scene.root, "appearanceWindowButtonSide_right")) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(rightSide, "click"));
    QTRY_VERIFY(scene.model->draftKeys.contains(QStringLiteral("appearance.windowButtonSide")));
    QCOMPARE(scene.model->draftValues.constLast().toString(), QStringLiteral("right"));

    QQuickItem *fromRight = nullptr;
    QTRY_VERIFY((fromRight = item(scene.root, "appearanceContainerTabOrder_right-to-left")) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(fromRight, "click"));
    QTRY_COMPARE(scene.model->draftKeys.constLast(),
                 QStringLiteral("appearance.containerTabOrder"));
    QCOMPARE(scene.model->draftValues.constLast().toString(), QStringLiteral("right-to-left"));
}

QTEST_MAIN(AppearancePageTests)
#include "tst_appearance_page.moc"
