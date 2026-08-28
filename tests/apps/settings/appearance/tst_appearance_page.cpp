// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"

#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QAccessibleInterface>
#include <QCoreApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QSignalSpy>
#include <QTest>
#include <QUrl>

#include <functional>
#include <memory>

using QindaQt::Apps::SettingsAppearance::ensureTokenFacade;

namespace {

constexpr auto BuildQmlImportPath = QINDAQT_QML_IMPORT_PATH;
constexpr auto SceneQmlDir = QINDAQT_APPEARANCE_TEST_QML_DIR;

// Duck-typed stand-in for AppearanceSettingsModel: the page is defined
// against this property surface, so the presentation test can drive every
// route without a live Settings1 lineage.
class StubAppearanceModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool loading MEMBER loading NOTIFY stateChanged)
    Q_PROPERTY(bool ready MEMBER ready NOTIFY stateChanged)
    Q_PROPERTY(bool saving MEMBER saving NOTIFY stateChanged)
    Q_PROPERTY(bool conflict MEMBER conflict NOTIFY stateChanged)
    Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY stateChanged)
    Q_PROPERTY(bool canEdit MEMBER canEdit NOTIFY stateChanged)
    Q_PROPERTY(bool draftDirty MEMBER draftDirty NOTIFY draftChanged)
    Q_PROPERTY(bool draftValid MEMBER draftValid NOTIFY draftChanged)
    Q_PROPERTY(bool applyAvailable MEMBER applyAvailable NOTIFY stateChanged)
    Q_PROPERTY(QString statusText MEMBER statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText MEMBER errorText NOTIFY stateChanged)
    Q_PROPERTY(QVariantMap draft MEMBER draft NOTIFY draftChanged)
    Q_PROPERTY(QVariantMap fieldErrors MEMBER fieldErrors NOTIFY draftChanged)
    Q_PROPERTY(QVariantList installedThemes MEMBER installedThemes CONSTANT)
    Q_PROPERTY(QString resolvedThemeId MEMBER resolvedThemeId NOTIFY draftChanged)
    Q_PROPERTY(bool configuredThemeInstalled MEMBER configuredThemeInstalled
                   NOTIFY draftChanged)
    Q_PROPERTY(QString fallbackNotice MEMBER fallbackNotice NOTIFY draftChanged)

public:
    explicit StubAppearanceModel(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE bool setDraftValue(const QString &key, const QVariant &value)
    {
        draftKeys.append(key);
        draftValues.append(value);
        draft.insert(key, value);
        Q_EMIT draftChanged();
        return true;
    }
    Q_INVOKABLE bool cancelDraft()
    {
        ++cancels;
        Q_EMIT draftChanged();
        return true;
    }
    Q_INVOKABLE bool applyDraft()
    {
        ++applies;
        return true;
    }
    Q_INVOKABLE void retry() { ++retries; }

    void publish()
    {
        Q_EMIT stateChanged();
        Q_EMIT draftChanged();
    }

    bool loading = true;
    bool ready = false;
    bool saving = false;
    bool conflict = false;
    bool unavailable = false;
    bool canEdit = false;
    bool draftDirty = false;
    bool draftValid = true;
    bool applyAvailable = false;
    bool configuredThemeInstalled = true;
    QString statusText = QStringLiteral("Loading appearance settings…");
    QString errorText;
    QString resolvedThemeId = QStringLiteral("qinda-dark");
    QString fallbackNotice;
    QVariantMap draft;
    QVariantMap fieldErrors;
    QVariantList installedThemes;
    QStringList draftKeys;
    QVariantList draftValues;
    int applies = 0;
    int cancels = 0;
    int retries = 0;

Q_SIGNALS:
    void stateChanged();
    void draftChanged();
};

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

struct Scene final {
    std::unique_ptr<QQuickView> view;
    std::unique_ptr<StubAppearanceModel> model;
    QQuickItem *root = nullptr;
};

Scene createScene(const std::function<void(StubAppearanceModel &)> &configure)
{
    Scene scene{.view = std::make_unique<QQuickView>(),
                .model = std::make_unique<StubAppearanceModel>()};
    if (configure) {
        configure(*scene.model);
    }
    scene.view->engine()->addImportPath(QStringLiteral(BuildQmlImportPath));
    QString error;
    auto *facade = ensureTokenFacade(*scene.view->engine(), &error);
    if (facade == nullptr) {
        qFatal("could not bind test tokens: %s", qPrintable(error));
    }
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!theme.ok || !facade->publish(theme.theme, {})) {
        qFatal("could not publish test tokens");
    }
    scene.view->setResizeMode(QQuickView::SizeRootObjectToView);
    scene.view->resize(640, 640);
    scene.view->setInitialProperties(
        {{QStringLiteral("stubModel"), QVariant::fromValue(scene.model.get())}});
    scene.view->setSource(QUrl::fromLocalFile(
        QStringLiteral(SceneQmlDir) + QStringLiteral("/AppearancePageScene.qml")));
    if (!scene.view->errors().isEmpty()) {
        qFatal("could not load appearance scene: %s",
               qPrintable(scene.view->errors().constFirst().toString()));
    }
    scene.view->show();
    QCoreApplication::processEvents();
    scene.root = scene.view->rootObject();
    if (scene.root == nullptr) {
        qFatal("appearance scene has no root item");
    }
    return scene;
}

QQuickItem *item(const QQuickItem *root, const char *name)
{
    return root->findChild<QQuickItem *>(QString::fromLatin1(name));
}

QAccessible::Role roleOf(QQuickItem *item_)
{
    auto *interface = QAccessible::queryAccessibleInterface(item_);
    if (interface == nullptr) {
        qFatal("missing accessible interface for %s",
               qPrintable(item_->objectName()));
    }
    return interface->role();
}

} // namespace

class AppearancePageTests final : public QObject {
    Q_OBJECT

private slots:
    void themeCardsRenderSelectAndGate();
    void actionRowWiresApplyRevertRetryClose();
    void statusFallbackAndAccessibilityTruth();
    void initialFocusAndTabTraversal();

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

    auto *darkCard = item(scene.root, "appearanceThemeCard_qinda-dark");
    auto *lightCard = item(scene.root, "appearanceThemeCard_qinda-light");
    QVERIFY(darkCard != nullptr);
    QVERIFY(lightCard != nullptr);
    QVERIFY(darkCard->isEnabled());
    QVERIFY(darkCard->property("checked").toBool());
    QVERIFY(!lightCard->property("checked").toBool());

    // Radio semantics: selecting the second card routes one typed draft write.
    lightCard->setChecked(true);
    QMetaObject::invokeMethod(lightCard, "toggled", Q_ARG(bool, true));
    QCOMPARE(scene.model->draftKeys.size(), 1);
    QCOMPARE(scene.model->draftKeys.constFirst(),
             QStringLiteral("appearance.theme"));
    QCOMPARE(scene.model->draftValues.constFirst().toString(),
             QStringLiteral("qinda-light"));

    // While saving, the fail-closed gate disables every theme card.
    scene.model->saving = true;
    scene.model->canEdit = false;
    scene.model->publish();
    QVERIFY(!darkCard->isEnabled());
    QVERIFY(!lightCard->isEnabled());
}

void AppearancePageTests::actionRowWiresApplyRevertRetryClose()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{};
        model.draft = defaultDraftMap();
        makeReady(model, true);
    });

    auto *apply = item(scene.root, "appearanceApplyButton");
    auto *revert = item(scene.root, "appearanceRevertButton");
    auto *close = item(scene.root, "appearanceCloseButton");
    auto *retry = item(scene.root, "appearanceRetryButton");
    QVERIFY(apply != nullptr && revert != nullptr && close != nullptr
            && retry != nullptr);
    QVERIFY(apply->isVisible());
    QVERIFY(revert->isVisible());
    QVERIFY(!retry->isVisible());

    bool closeRequested = false;
    const QMetaObject::Connection closed = connect(
        scene.root, SIGNAL(closeRequested()), this,
        [&closeRequested]() { closeRequested = true; });
    QVERIFY(closed);

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
    QVERIFY(!closeRequested);

    // Close routes through the scene's closeRequested signal.
    scene.model->unavailable = false;
    scene.model->ready = true;
    scene.model->canEdit = true;
    scene.model->publish();
    QMetaObject::invokeMethod(close, "clicked");
    QVERIFY(closeRequested);
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

void AppearancePageTests::initialFocusAndTabTraversal()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{themeEntry(
            QStringLiteral("qinda-dark"), QStringLiteral("Qinda Dark"),
            QStringLiteral("dark"),
            previewTokensFor(QStringLiteral("qinda-dark.json")))};
        model.draft = defaultDraftMap();
        makeReady(model, false);
    });

    auto *firstCard = item(scene.root, "appearanceThemeCard_qinda-dark");
    QVERIFY(firstCard != nullptr);
    QTRY_VERIFY(firstCard->hasActiveFocus());

    // The default tab chain leaves the theme grid toward the form controls.
    QTest::keyClick(scene.view.get(), Qt::Key_Tab);
    QVERIFY(!firstCard->hasActiveFocus());
    QVERIFY(scene.view->activeFocusItem() != nullptr);
}

QTEST_MAIN(AppearancePageTests)
#include "tst_appearance_page.moc"
