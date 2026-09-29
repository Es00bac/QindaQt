// SPDX-License-Identifier: GPL-3.0-or-later
// ADR-0286: the Wallpaper destination's scope picker — display thumbnails,
// the desktop selector, where a gallery pick lands, the "stop using a
// separate wallpaper" action, the saved-choices list, keyboard and
// accessible names, and a selection that falls back when its display is
// unplugged. The route model is the duck-typed stub; the target catalog is
// the real one.
#include "stub_appearance_model.h"
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/apps/settings_appearance/wallpaper_target_catalog.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QCoreApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>
#include <QUrl>

#include <functional>
#include <memory>

using QindaQt::Apps::SettingsAppearance::WallpaperTargetCatalog;

namespace {

const QString kStudio = QStringLiteral("edid:00112233445566778899aabbccddeeff");
const QString kTwin = QStringLiteral("edid:0011#2");
const QString kWork = QStringLiteral("desktop-work");
const QString kPunk = QStringLiteral("qindaqt:qinda-punk");

// AGENT-CONTRACT: members are declared model-first so reverse destruction
// tears the view down before the stub model and the catalog die.
struct Scene final {
    std::unique_ptr<WallpaperTargetCatalog> targets;
    std::unique_ptr<StubAppearanceModel> model;
    std::unique_ptr<QQuickView> view;
    QQuickItem *root = nullptr;
    QString error;
};

QVariantMap defaultDraft()
{
    return {{QStringLiteral("appearance.theme"), QStringLiteral("qinda-dark")},
            {QStringLiteral("appearance.colorScheme"), QStringLiteral("system")},
            {QStringLiteral("fonts.family"), QStringLiteral("Noto Sans")},
            {QStringLiteral("fonts.monospaceFamily"), QStringLiteral("Noto Sans Mono")},
            {QStringLiteral("fonts.pointSize"), 10.0},
            {QStringLiteral("fonts.antialiasing"), true},
            {QStringLiteral("fonts.hinting"), QStringLiteral("slight")},
            {QStringLiteral("fonts.subpixelOrder"), QStringLiteral("rgb")},
            {QStringLiteral("appearance.wallpaper"), QStringLiteral("qindaqt:jade-fold")},
            {QStringLiteral("appearance.wallpaperMode"), QStringLiteral("scaled")},
            {QStringLiteral("appearance.uiScale"), 1.0}};
}

void populate(WallpaperTargetCatalog &targets)
{
    QindaQt::Display::Output studio;
    studio.stableId = kStudio;
    studio.connectorName = QStringLiteral("DP-1");
    studio.label = QStringLiteral("Studio");
    studio.enabled = true;
    studio.primary = true;
    studio.logicalSize = QSize(2560, 1440);
    QindaQt::Display::Output twin = studio;
    twin.stableId = kTwin;
    twin.connectorName = QStringLiteral("DP-2");
    twin.ambiguousIdentity = true;
    twin.primary = false;
    twin.position = QPoint(2560, 0);
    twin.logicalSize = QSize(1920, 1080);
    targets.setDisplayOutputs({studio, twin});
    targets.setDesktopRows({QVariantMap{{QStringLiteral("id"), kWork},
                                        {QStringLiteral("name"), QStringLiteral("Work")}}});
}

Scene createScene(bool withTargets, const std::function<void(StubAppearanceModel &)> &configure)
{
    Scene scene;
    if (withTargets) {
        scene.targets = std::make_unique<WallpaperTargetCatalog>();
        populate(*scene.targets);
    }
    scene.model = std::make_unique<StubAppearanceModel>();
    scene.model->installedThemes = QVariantList{};
    scene.model->bundledWallpapers = QVariantList{QVariantMap{
        {QStringLiteral("name"), QStringLiteral("Qinda punk")},
        {QStringLiteral("value"), kPunk},
        {QStringLiteral("previewUrl"),
         QUrl::fromLocalFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/wallpapers/qinda-punk.png"))}}};
    scene.model->draft = defaultDraft();
    scene.model->wallpaperTargets = scene.targets.get();
    scene.model->loading = false;
    scene.model->ready = true;
    scene.model->hasConfirmed = true;
    scene.model->canEdit = true;
    scene.model->statusText.clear();
    if (configure)
        configure(*scene.model);
    scene.view = std::make_unique<QQuickView>();
    scene.view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(*scene.view->engine(),
                                                                        &error);
    if (!QindaQt::Shell::Icons::IconRuntime::install(
            *scene.view->engine(), {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
            {QStringLiteral("QindaQt")})) {
        scene.error = QStringLiteral("icon runtime install failed");
        return scene;
    }
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (facade == nullptr || !theme.ok || !facade->publish(theme.theme, {})) {
        scene.error = QStringLiteral("could not publish test tokens: %1").arg(error);
        return scene;
    }
    scene.view->setResizeMode(QQuickView::SizeRootObjectToView);
    scene.view->resize(900, 900);
    scene.view->setInitialProperties(
        {{QStringLiteral("stubModel"), QVariant::fromValue(scene.model.get())}});
    scene.view->setSource(QUrl::fromLocalFile(QStringLiteral(QINDAQT_APPEARANCE_TEST_QML_DIR)
                                              + QStringLiteral("/AppearancePageScene.qml")));
    if (!scene.view->errors().isEmpty()) {
        scene.error = scene.view->errors().constFirst().toString();
        return scene;
    }
    scene.view->show();
    QCoreApplication::processEvents();
    scene.root = scene.view->rootObject();
    return scene;
}

QQuickItem *item(QQuickItem *root, const QString &wanted)
{
    if (root->objectName() == wanted)
        return root;
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *match = item(child, wanted))
            return match;
    }
    return nullptr;
}

// The Wallpaper destination's section (the page Loader's item).
QQuickItem *openWallpaper(const Scene &scene)
{
    QQuickItem *tab = item(scene.root, QStringLiteral("appearanceDestination_wallpaper"));
    if (tab == nullptr || !QMetaObject::invokeMethod(tab, "click"))
        return nullptr;
    QCoreApplication::processEvents();
    QQuickItem *loader = item(scene.root, QStringLiteral("appearanceDestinationPage_wallpaper"));
    return loader == nullptr ? nullptr : loader->property("item").value<QQuickItem *>();
}

QString accessibleName(QQuickItem *control)
{
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(control);
    return interface == nullptr ? QString() : interface->text(QAccessible::Name);
}

bool click(QQuickItem *control)
{
    return control != nullptr && QMetaObject::invokeMethod(control, "click");
}

} // namespace

class AppearanceWallpaperScopesPageTests final : public QObject {
    Q_OBJECT

private slots:
    void withoutTargetsThePageStaysEverywhereOnly();
    void picksLandOnTheChosenDisplayAndDesktop();
    void savedChoicesListAndUnpluggedSelectionFallBack();
};

void AppearanceWallpaperScopesPageTests::withoutTargetsThePageStaysEverywhereOnly()
{
    const Scene scene = createScene(false, {});
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    QQuickItem *section = openWallpaper(scene);
    QVERIFY(section != nullptr);
    QQuickItem *all = item(scene.root, QStringLiteral("appearanceWallpaperAllDisplays"));
    QVERIFY(all != nullptr);
    QVERIFY(!all->isVisible());
    QVERIFY(click(item(scene.root, QStringLiteral("bundledWallpaperButton"))));
    QTRY_COMPARE(scene.model->draft.value(QStringLiteral("appearance.wallpaper")).toString(), kPunk);
    QVERIFY(scene.model->wallpaperCalls.isEmpty());
}

void AppearanceWallpaperScopesPageTests::picksLandOnTheChosenDisplayAndDesktop()
{
    const Scene scene = createScene(true, {});
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    QQuickItem *section = openWallpaper(scene);
    QVERIFY(section != nullptr);

    QQuickItem *all = item(scene.root, QStringLiteral("appearanceWallpaperAllDisplays"));
    QQuickItem *studio = item(scene.root, QStringLiteral("appearanceWallpaperDisplay_") + kStudio);
    QQuickItem *twin = item(scene.root, QStringLiteral("appearanceWallpaperDisplay_") + kTwin);
    QQuickItem *desktops = item(scene.root, QStringLiteral("appearanceWallpaperDesktopSelector"));
    QQuickItem *summary = item(scene.root, QStringLiteral("appearanceWallpaperScopeSummary"));
    QVERIFY(all != nullptr && studio != nullptr && twin != nullptr && desktops != nullptr
            && summary != nullptr);
    QTRY_VERIFY(all->isVisible() && studio->isVisible() && desktops->isVisible());
    // Every scope control is a named, keyboard-reachable control; the twin
    // display cannot be told apart and is offered but not selectable.
    QCOMPARE(accessibleName(studio), QStringLiteral("Display 1, Studio, primary"));
    QVERIFY(studio->activeFocusOnTab());
    QVERIFY(studio->isEnabled());
    QVERIFY(!twin->isEnabled());
    QCOMPARE(section->property("selectedDisplay").toString(), QString());

    QVERIFY(click(studio));
    QTRY_COMPARE(section->property("selectedDisplay").toString(), kStudio);
    QVERIFY(summary->property("text").toString().startsWith(QStringLiteral("Display 1 uses")));
    QVERIFY(click(item(scene.root, QStringLiteral("bundledWallpaperButton"))));
    QTRY_COMPARE(scene.model->wallpaperCalls.size(), 1);
    QCOMPARE(scene.model->wallpaperCalls.constLast(), (QStringList{kStudio, QString(), kPunk}));
    // The everywhere wallpaper is untouched by a per-display pick.
    QCOMPARE(scene.model->draft.value(QStringLiteral("appearance.wallpaper")).toString(),
             QStringLiteral("qindaqt:jade-fold"));
    QTRY_VERIFY(summary->property("text").toString().startsWith(QStringLiteral("Display 1: ")));

    // Desktop selector: Work on Display 1, then "No wallpaper" there.
    QVERIFY(QMetaObject::invokeMethod(desktops, "activated", Q_ARG(int, 1)));
    QTRY_COMPARE(section->property("selectedDesktop").toString(), kWork);
    QVERIFY(click(item(scene.root, QStringLiteral("noWallpaperButton"))));
    QTRY_COMPARE(scene.model->wallpaperCalls.size(), 2);
    QCOMPARE(scene.model->wallpaperCalls.constLast(), (QStringList{kStudio, kWork, QString()}));

    // The scope now has its own choice; the follow action removes it.
    QQuickItem *follow = item(scene.root, QStringLiteral("appearanceWallpaperFollowButton"));
    QVERIFY(follow != nullptr);
    QTRY_VERIFY(follow->isVisible());
    QVERIFY(click(follow));
    QTRY_COMPARE(scene.model->clearCalls.size(), 1);
    QCOMPARE(scene.model->clearCalls.constLast(), (QStringList{kStudio, kWork}));
    QTRY_VERIFY(!follow->isVisible());

    // Every display on the Work desktop.
    QVERIFY(click(all));
    QTRY_COMPARE(section->property("selectedDisplay").toString(), QString());
    QVERIFY(click(item(scene.root, QStringLiteral("bundledWallpaperButton"))));
    QTRY_COMPARE(scene.model->wallpaperCalls.size(), 3);
    QCOMPARE(scene.model->wallpaperCalls.constLast(), (QStringList{QString(), kWork, kPunk}));

    // Back to every desktop: the gallery edits the everywhere wallpaper again.
    QVERIFY(QMetaObject::invokeMethod(desktops, "activated", Q_ARG(int, 0)));
    QTRY_COMPARE(section->property("selectedDesktop").toString(), QString());
    QVERIFY(click(item(scene.root, QStringLiteral("noWallpaperButton"))));
    QTRY_COMPARE(scene.model->draft.value(QStringLiteral("appearance.wallpaper")).toString(),
                 QString());
    QCOMPARE(scene.model->wallpaperCalls.size(), 3);
}

void AppearanceWallpaperScopesPageTests::savedChoicesListAndUnpluggedSelectionFallBack()
{
    const QString gone = QStringLiteral("edid:ffeeddccbbaa99887766554433221100");
    const Scene scene = createScene(true, [&gone](StubAppearanceModel &model) {
        model.wallpaperAssignmentRows = QVariantList{QVariantMap{
            {QStringLiteral("display"), gone},
            {QStringLiteral("desktop"), QString()},
            {QStringLiteral("wallpaper"), QStringLiteral("/home/user/old.png")},
            {QStringLiteral("displayPresent"), false},
            {QStringLiteral("desktopPresent"), true},
            {QStringLiteral("displayLabel"), QStringLiteral("A display that is not connected")},
            {QStringLiteral("desktopLabel"), QStringLiteral("all desktops")},
            {QStringLiteral("wallpaperLabel"), QStringLiteral("old.png")}}};
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    QQuickItem *section = openWallpaper(scene);
    QVERIFY(section != nullptr);

    QQuickItem *remove = item(scene.root, QStringLiteral("appearanceWallpaperRemoveChoice"));
    QVERIFY(remove != nullptr);
    QTRY_VERIFY(remove->isVisible());
    QVERIFY(accessibleName(remove).contains(QStringLiteral("Remove")));
    QVERIFY(click(remove));
    QTRY_COMPARE(scene.model->clearCalls.size(), 1);
    QCOMPARE(scene.model->clearCalls.constLast(), (QStringList{gone, QString()}));

    // A selected display that is unplugged falls back to every display, so a
    // pick never edits a scope the page no longer shows.
    QVERIFY(click(item(scene.root, QStringLiteral("appearanceWallpaperDisplay_") + kStudio)));
    QTRY_COMPARE(section->property("selectedDisplay").toString(), kStudio);
    scene.targets->setDisplayOutputs({});
    QTRY_COMPARE(section->property("selectedDisplay").toString(), QString());
    QVERIFY(click(item(scene.root, QStringLiteral("bundledWallpaperButton"))));
    QTRY_COMPARE(scene.model->draft.value(QStringLiteral("appearance.wallpaper")).toString(), kPunk);
    QVERIFY(scene.model->wallpaperCalls.isEmpty());
}

QTEST_MAIN(AppearanceWallpaperScopesPageTests)
#include "tst_appearance_wallpaper_scopes_page.moc"
