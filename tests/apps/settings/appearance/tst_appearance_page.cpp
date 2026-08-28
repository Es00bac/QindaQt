// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"

#include "qindaqt/design_tokens/design_tokens.h"
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

const char *const BuildQmlImportPath = QINDAQT_QML_IMPORT_PATH;
const char *const SceneQmlDir = QINDAQT_APPEARANCE_TEST_QML_DIR;

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

QQuickItem *item(const QQuickItem *root, const char *name)
{
    const QString wanted = QString::fromLatin1(name);
    if (root->objectName() == wanted) {
        return const_cast<QQuickItem *>(root);
    }
    // Repeater delegates are visual children but are not guaranteed to use
    // the containing item as their QObject parent. Traverse the scene graph,
    // which is the ownership relation this presentation test exercises.
    for (QQuickItem *child : root->childItems()) {
        if (auto *match = item(child, name); match != nullptr) {
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

    // Radio semantics: selecting the second card routes one typed draft write.
    QVERIFY(QMetaObject::invokeMethod(lightCard, "click"));
    QTRY_COMPARE(scene.model->draftKeys.size(), 1);
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

void AppearancePageTests::toggleHandlersForwardAuthoritativeCheckedValues()
{
    const auto scene = createScene([](StubAppearanceModel &model) {
        model.installedThemes = QVariantList{};
        model.draft = defaultDraftMap();
        makeReady(model, false);
    });
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));

    auto *antialiasing = item(scene.root, "appearanceAntialiasingSwitch");
    auto *darkScheme = item(scene.root, "appearanceSchemeButton_dark");
    QVERIFY(antialiasing != nullptr);
    QVERIFY(darkScheme != nullptr);

    // QQuickAbstractButton::toggled() carries no Boolean argument. The QML
    // handlers must read each control's checked property after an ordinary
    // click instead of treating an absent signal parameter as false.
    QVERIFY(QMetaObject::invokeMethod(antialiasing, "click"));
    QTRY_COMPARE(scene.model->draftKeys.size(), 1);
    QCOMPARE(scene.model->draftKeys.constLast(),
             QStringLiteral("fonts.antialiasing"));
    QCOMPARE(scene.model->draftValues.constLast().toBool(), false);

    QVERIFY(QMetaObject::invokeMethod(darkScheme, "click"));
    QTRY_COMPARE(scene.model->draftKeys.size(), 2);
    QCOMPARE(scene.model->draftKeys.constLast(),
             QStringLiteral("appearance.colorScheme"));
    QCOMPARE(scene.model->draftValues.constLast().toString(),
             QStringLiteral("dark"));
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
    auto *close = item(scene.root, "appearanceCloseButton");
    auto *retry = item(scene.root, "appearanceRetryButton");
    QVERIFY(apply != nullptr && revert != nullptr && close != nullptr
            && retry != nullptr);
    QVERIFY(apply->isVisible());
    QVERIFY(revert->isVisible());
    QVERIFY(!retry->isVisible());

    QSignalSpy closeSpy(scene.root, SIGNAL(closeRequested()));
    QVERIFY(closeSpy.isValid());

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
    QCOMPARE(closeSpy.size(), 0);

    // Close routes through the scene's closeRequested signal.
    scene.model->unavailable = false;
    scene.model->ready = true;
    scene.model->canEdit = true;
    scene.model->publish();
    QMetaObject::invokeMethod(close, "clicked");
    QTRY_COMPARE(closeSpy.size(), 1);
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
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));

    QQuickItem *firstCard = nullptr;
    QVERIFY2(QTest::qWaitFor(
                 [&]() {
                     firstCard = item(scene.root,
                                      "appearanceThemeCard_qinda-dark");
                     return firstCard != nullptr;
                 },
                 1'000),
             qPrintable(QStringLiteral("first theme card missing; descendants: %1")
                            .arg(descendantObjectNames(scene.root))));
    QTRY_VERIFY(firstCard->hasActiveFocus());

    // The default tab chain leaves the theme grid toward the form controls.
    QTest::keyClick(scene.view.get(), Qt::Key_Tab);
    QVERIFY(!firstCard->hasActiveFocus());
    QVERIFY(scene.view->activeFocusItem() != nullptr);
}

QTEST_MAIN(AppearancePageTests)
#include "tst_appearance_page.moc"
