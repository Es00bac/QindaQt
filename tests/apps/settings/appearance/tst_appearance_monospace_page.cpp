// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_appearance_model.h"
#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/apps/settings_appearance/appearance_values.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/themes/theme_loader.h"

#include <QCoreApplication>
#include <QFont>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickView>
#include <QTest>
#include <QUrl>

#include <memory>

using QindaQt::Apps::SettingsAppearance::AppearanceValues;
using QindaQt::Apps::SettingsAppearance::ensureTokenFacade;

namespace {
QQuickItem *item(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) {
        if (auto *found = item(child, name)) return found;
    }
    return nullptr;
}

struct Scene final {
    // The view must die before its QML binding target.
    std::unique_ptr<StubAppearanceModel> model;
    std::unique_ptr<QQuickView> view;
    QQuickItem *root = nullptr;
    QString error;
};

Scene createScene()
{
    Scene scene;
    scene.model = std::make_unique<StubAppearanceModel>();
    scene.model->draft = AppearanceValues{}.toVariantMap();
    scene.model->loading = false;
    scene.model->ready = true;
    scene.model->hasConfirmed = true;
    scene.model->canEdit = true;
    scene.model->statusText.clear();
    scene.view = std::make_unique<QQuickView>();
    scene.view->engine()->addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
    QString error;
    auto *facade = ensureTokenFacade(*scene.view->engine(), &error);
    if (facade == nullptr) {
        scene.error = error;
        return scene;
    }
    if (!QindaQt::Shell::Icons::IconRuntime::install(
            *scene.view->engine(),
            {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
            {QStringLiteral("QindaQt")})) {
        scene.error = QStringLiteral("icon runtime install failed");
        return scene;
    }
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    if (!theme.ok || !facade->publish(theme.theme, {})) {
        scene.error = QStringLiteral("theme publication failed");
        return scene;
    }
    scene.view->setResizeMode(QQuickView::SizeRootObjectToView);
    scene.view->resize(420, 320);
    scene.view->setInitialProperties(
        {{QStringLiteral("stubModel"), QVariant::fromValue(scene.model.get())}});
    scene.view->setSource(QUrl::fromLocalFile(
        QStringLiteral(QINDAQT_APPEARANCE_TEST_QML_DIR "/AppearancePageScene.qml")));
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

class AppearanceMonospacePageTests final : public QObject {
    Q_OBJECT
private slots:
    void keyboardEditingKeepsFamiliesIndependentAndShowsSavedState();
    void typedUnknownFamilyReachesSharedDraft();
};

void AppearanceMonospacePageTests::keyboardEditingKeepsFamiliesIndependentAndShowsSavedState()
{
    const auto scene = createScene();
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    auto *tab = item(scene.root, QStringLiteral("appearanceDestination_fonts"));
    QVERIFY(tab != nullptr);
    QVERIFY(QMetaObject::invokeMethod(tab, "click"));
    QTest::qWait(20); // Let the destination's deferred initial focus settle.
    QQuickItem *mono = nullptr;
    QTRY_VERIFY((mono = item(scene.root, QStringLiteral("appearanceMonospaceFamilyField"))) != nullptr);
    auto *state = item(scene.root, QStringLiteral("appearanceMonospaceState"));
    auto *preview = item(scene.root, QStringLiteral("appearanceMonospacePreview"));
    QVERIFY(state != nullptr);
    QVERIFY(preview != nullptr);
    QCOMPARE(mono->property("model").toStringList(),
             (QStringList{QStringLiteral("Noto Sans Mono"),
                          QStringLiteral("Liberation Mono")}));
    mono->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(mono->hasActiveFocus());
    QTest::keyClick(scene.view.get(), Qt::Key_Down);
    QTest::keyClick(scene.view.get(), Qt::Key_Return);
    QTRY_COMPARE(scene.model->draft.value(QStringLiteral("fonts.monospaceFamily"))
                     .toString(), QStringLiteral("Liberation Mono"));
    QCOMPARE(scene.model->draft.value(QStringLiteral("fonts.family")).toString(),
             QStringLiteral("Noto Sans"));
    QTRY_VERIFY(state->property("text").toString().contains(
        QStringLiteral("Saved: Noto Sans Mono")));
    QTRY_VERIFY(state->property("text").toString().contains(
        QStringLiteral("Draft: Liberation Mono")));
    QTRY_COMPARE(preview->property("font").value<QFont>().family(),
                 QStringLiteral("Liberation Mono"));
    scene.model->confirmedMonospaceFamily = QStringLiteral("Liberation Mono");
    scene.model->publish();
    QTRY_VERIFY(state->property("text").toString().contains(
        QStringLiteral("Saved: Liberation Mono")));
}

void AppearanceMonospacePageTests::typedUnknownFamilyReachesSharedDraft()
{
    const auto scene = createScene();
    QVERIFY2(scene.root != nullptr, qPrintable(scene.error));
    auto *tab = item(scene.root, QStringLiteral("appearanceDestination_fonts"));
    QVERIFY(tab != nullptr);
    QVERIFY(QMetaObject::invokeMethod(tab, "click"));
    QTest::qWait(20); // Let the destination's deferred initial focus settle.
    QQuickItem *mono = nullptr;
    QTRY_VERIFY((mono = item(scene.root, QStringLiteral("appearanceMonospaceFamilyField"))) != nullptr);
    auto *editor = qobject_cast<QQuickItem *>(
        mono->property("contentItem").value<QObject *>());
    QVERIFY(editor != nullptr);
    editor->forceActiveFocus(Qt::OtherFocusReason);
    QTRY_COMPARE(scene.view->activeFocusItem(), editor);
    for (const auto key : {Qt::Key_Space, Qt::Key_S, Qt::Key_E, Qt::Key_R,
                           Qt::Key_I, Qt::Key_F}) {
        QTest::keyClick(scene.view.get(), key);
    }
    QTRY_COMPARE(scene.model->draft.value(QStringLiteral("fonts.monospaceFamily"))
                     .toString(), QStringLiteral("serif"));
    QCOMPARE(scene.model->draft.value(QStringLiteral("fonts.family")).toString(),
             QStringLiteral("Noto Sans"));
}

QTEST_MAIN(AppearanceMonospacePageTests)
#include "tst_appearance_monospace_page.moc"
