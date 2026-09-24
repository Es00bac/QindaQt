// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_appearance_model.h"
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/themes/theme_loader.h"

#include <QCoreApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsAppearance::ensureTokenFacade;

namespace {

QQuickItem *item(const QQuickItem *root, const QString &wanted)
{
    if (root->objectName() == wanted) {
        return const_cast<QQuickItem *>(root);
    }
    for (QQuickItem *child : root->childItems()) {
        if (auto *match = item(child, wanted); match != nullptr) {
            return match;
        }
    }
    return nullptr;
}

struct Scene final {
    std::unique_ptr<StubAppearanceModel> model;
    std::unique_ptr<QQuickView> view;
    QQuickItem *root = nullptr;
    QString error;
};

Scene createScene()
{
    Scene scene;
    scene.model = std::make_unique<StubAppearanceModel>();
    scene.model->loading = false;
    scene.model->ready = true;
    scene.model->canEdit = true;
    scene.model->statusText.clear();
    scene.model->draft = {
        {QStringLiteral("appearance.windowButtonStyle"), QStringLiteral("theme")},
        {QStringLiteral("appearance.windowButtonSide"), QStringLiteral("theme")},
        {QStringLiteral("appearance.windowButtons"), QStringLiteral("all")},
        {QStringLiteral("appearance.windowTitleAlignment"), QStringLiteral("center")},
        {QStringLiteral("appearance.containerButtonStyle"), QStringLiteral("theme")},
        {QStringLiteral("appearance.containerButtonSide"), QStringLiteral("theme")},
        {QStringLiteral("appearance.containerTabOrder"), QStringLiteral("theme")},
        {QStringLiteral("appearance.containerButtonGlyphs"), QStringLiteral("theme")},
        {QStringLiteral("appearance.windowDecoration"), QStringLiteral("theme")},
        {QStringLiteral("appearance.containerDecoration"), QStringLiteral("theme")},
    };

    scene.view = std::make_unique<QQuickView>();
    scene.view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = ensureTokenFacade(*scene.view->engine(), &error);
    const bool iconsReady = QindaQt::Shell::Icons::IconRuntime::install(
        *scene.view->engine(), {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
        {QStringLiteral("QindaQt")});
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (facade == nullptr || !iconsReady || !theme.ok
        || !facade->publish(theme.theme, {})) {
        scene.error = QStringLiteral("could not initialize decoration page scene: %1")
                          .arg(error);
        return scene;
    }
    scene.view->setResizeMode(QQuickView::SizeRootObjectToView);
    scene.view->resize(640, 640);
    scene.view->setInitialProperties(
        {{QStringLiteral("stubModel"), QVariant::fromValue(scene.model.get())}});
    scene.view->setSource(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_APPEARANCE_TEST_QML_DIR
                       "/AppearancePageScene.qml")));
    if (!scene.view->errors().isEmpty()) {
        scene.error = scene.view->errors().constFirst().toString();
        return scene;
    }
    scene.view->show();
    QCoreApplication::processEvents();
    scene.root = scene.view->rootObject();
    return scene;
}

} // namespace

class WindowDecorationPageTests final : public QObject {
    Q_OBJECT

private slots:
    void listsAppliesAndGatesForeignDecorationPreview();
    void offersEveryButtonStyleAndTitleBarOption();
};

void WindowDecorationPageTests::listsAppliesAndGatesForeignDecorationPreview()
{
    const auto scene = createScene();
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    auto *destination = item(scene.root, QStringLiteral("appearanceDestination_windows"));
    QVERIFY(destination != nullptr);
    QVERIFY(QMetaObject::invokeMethod(destination, "click"));
    QCoreApplication::processEvents();

    auto *repeater = scene.root->findChild<QObject *>(
        QStringLiteral("appearanceDecorationRepeater"));
    auto *scratchy = item(scene.root,
                          QStringLiteral("appearanceDecoration_aurorae:Scratchy"));
    auto *apply = item(scene.root, QStringLiteral("appearanceApplyDecoration"));
    auto *external = item(scene.root,
                          QStringLiteral("appearanceExternalDecorationPreview"));
    auto *qindaPreview = item(scene.root,
                              QStringLiteral("appearanceWindowChromePreview"));
    QVERIFY(repeater != nullptr);
    QCOMPARE(repeater->property("count").toInt(), 2);
    QVERIFY(scratchy != nullptr && apply != nullptr);
    QVERIFY(external != nullptr && qindaPreview != nullptr);

    QVERIFY(QMetaObject::invokeMethod(scratchy, "click"));
    QTRY_COMPARE(scene.model->windowDecorations.selectedId,
                 QStringLiteral("aurorae:Scratchy"));
    QTRY_VERIFY(external->isVisible());
    QTRY_VERIFY(!qindaPreview->isVisible());
    QVERIFY(apply->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(apply, "click"));
    QTRY_COMPARE(scene.model->windowDecorations.applies, 1);
    QCOMPARE(scene.model->windowDecorations.configuredId,
             QStringLiteral("aurorae:Scratchy"));
}

void WindowDecorationPageTests::offersEveryButtonStyleAndTitleBarOption()
{
    // ADR-0264: windows and containers list the same fifteen button styles
    // in a menu that follows the draft, and every title-bar option row
    // forwards its token to the one draft.
    const auto scene = createScene();
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    auto *destination = item(scene.root, QStringLiteral("appearanceDestination_windows"));
    QVERIFY(destination != nullptr);
    QVERIFY(QMetaObject::invokeMethod(destination, "click"));
    QCoreApplication::processEvents();

    QQuickItem *windowStyles = nullptr;
    QTRY_VERIFY((windowStyles = item(scene.root, QStringLiteral("appearanceWindowButtonStyle")))
                != nullptr);
    auto *containerStyles = item(scene.root, QStringLiteral("appearanceContainerButtonStyle"));
    QVERIFY(containerStyles != nullptr);
    QCOMPARE(windowStyles->property("count").toInt(), 15);
    QCOMPARE(containerStyles->property("count").toInt(), 15);
    QCOMPARE(windowStyles->property("currentIndex").toInt(), 0);
    const auto indexOf = [](QQuickItem *menu, const QString &token) {
        int index = -1;
        QMetaObject::invokeMethod(menu, "indexOfValue", Q_RETURN_ARG(int, index),
                                  Q_ARG(QVariant, token));
        return index;
    };
    const int gel = indexOf(windowStyles, QStringLiteral("gel"));
    QVERIFY(gel > 0);
    QVERIFY(QMetaObject::invokeMethod(windowStyles, "activated", Q_ARG(int, gel)));
    QTRY_COMPARE(scene.model->draft.value(QStringLiteral("appearance.windowButtonStyle"))
                     .toString(),
                 QStringLiteral("gel"));
    QTRY_COMPARE(windowStyles->property("currentIndex").toInt(), gel);
    const int pills = indexOf(containerStyles, QStringLiteral("pills"));
    QVERIFY(pills > 0);
    QVERIFY(QMetaObject::invokeMethod(containerStyles, "activated", Q_ARG(int, pills)));
    QTRY_COMPARE(scene.model->draft.value(QStringLiteral("appearance.containerButtonStyle"))
                     .toString(),
                 QStringLiteral("pills"));

    const QList<std::pair<QString, QString>> rows{
        {QStringLiteral("appearanceWindowButtonSize_large"),
         QStringLiteral("appearance.windowButtonSize")},
        {QStringLiteral("appearanceWindowButtonSpacing_roomy"),
         QStringLiteral("appearance.windowButtonSpacing")},
        {QStringLiteral("appearanceWindowTitleHeight_tall"),
         QStringLiteral("appearance.windowTitleHeight")},
        {QStringLiteral("appearanceWindowCornerRadius_square"),
         QStringLiteral("appearance.windowCornerRadius")},
        {QStringLiteral("appearanceWindowTitleWeight_bold"),
         QStringLiteral("appearance.windowTitleWeight")},
        {QStringLiteral("appearanceWindowAppIcon_shown"),
         QStringLiteral("appearance.windowAppIcon")},
        {QStringLiteral("appearanceWindowRollUpButton_shown"),
         QStringLiteral("appearance.windowRollUpButton")},
        {QStringLiteral("appearanceWindowTitleDoubleClick_roll-up"),
         QStringLiteral("appearance.windowTitleDoubleClick")},
        {QStringLiteral("appearanceContainerButtonSize_small"),
         QStringLiteral("appearance.containerButtonSize")},
        {QStringLiteral("appearanceContainerButtonSpacing_tight"),
         QStringLiteral("appearance.containerButtonSpacing")},
        {QStringLiteral("appearanceContainerTitleDoubleClick_maximize"),
         QStringLiteral("appearance.containerTitleDoubleClick")},
    };
    for (const auto &[objectName, key] : rows) {
        QQuickItem *choice = nullptr;
        QTRY_VERIFY2((choice = item(scene.root, objectName)) != nullptr, qPrintable(objectName));
        QVERIFY(QMetaObject::invokeMethod(choice, "click"));
        const QString token = objectName.section(QLatin1Char('_'), 1);
        QTRY_COMPARE(scene.model->draft.value(key).toString(), token);
        QTRY_VERIFY2(choice->property("checked").toBool(), qPrintable(objectName));
    }
    // A container bar's height belongs to the container layout: no option.
    QVERIFY(item(scene.root, QStringLiteral("appearanceContainerTitleHeight_tall")) == nullptr);
}

QTEST_MAIN(WindowDecorationPageTests)
#include "tst_window_decoration_page.moc"
