// SPDX-License-Identifier: GPL-3.0-or-later
// The Settings Customize preset page (ADR-0267), warning-fatal and offscreen,
// against the stub model: built-ins before own presets, accessible cards with
// text badges, switching by pointer and keyboard, the name dialog's
// validation, confirmed delete and restore, the degraded notice, and the
// global auto-hide delay control.
#include "stub_customize_settings_model.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QKeyEvent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickView>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)
Q_IMPORT_QML_PLUGIN(QindaQtSettingsCustomizePlugin)

using QindaQt::Apps::SettingsCustomize::TestSupport::StubCustomizeSettingsModel;

namespace {

QQuickItem *item(QQuickItem *root, const char *name)
{
    if (root == nullptr) {
        return nullptr;
    }
    if (root->objectName() == QLatin1String(name)) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (auto *match = item(child, name); match != nullptr) {
            return match;
        }
    }
    return nullptr;
}

// Popup content lives under the window's overlay, not under the page, so
// dialog controls are searched from the window content item.
QQuickItem *windowItem(QQuickView &view, const char *name)
{
    return item(view.contentItem(), name);
}

QString accessibleName(QQuickItem *candidate)
{
    if (candidate == nullptr) {
        return {};
    }
    QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(candidate);
    return interface == nullptr ? QString{} : interface->text(QAccessible::Name);
}

bool accessibleChecked(QQuickItem *candidate)
{
    QAccessibleInterface *interface = candidate == nullptr
        ? nullptr : QAccessible::queryAccessibleInterface(candidate);
    return interface != nullptr && interface->state().checked;
}

void click(QQuickView &view, QQuickItem *target)
{
    QVERIFY(target != nullptr);
    QVERIFY(target->isVisible());
    const QPoint centre = target->mapToScene(
        QPointF(target->width() / 2, target->height() / 2)).toPoint();
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, centre);
}

QObject *popup(QQuickView &view, const char *name)
{
    return view.rootObject()->findChild<QObject *>(QLatin1String(name));
}

bool opened(QQuickView &view, const char *name)
{
    QObject *dialog = popup(view, name);
    return dialog != nullptr && dialog->property("opened").toBool();
}

// Shared production-page load: token facade, the shipped icon theme, and the
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
    void rendersBuiltInsThenOwnPresetsAccessibly();
    void cardsSwitchByPointerAndKeyboard();
    void saveCurrentLayoutValidatesTheName();
    void ownPresetsRenameDuplicateAndDeleteAfterConfirmation();
    void editedBuiltInRestoresAfterConfirmationOrSavesAsNew();
    void busyAndUnavailableStatesStayTruthful();
    void panelHideDelayCommitsFinalPointerAndKeyboardIntent();
};

void CustomizePageTests::rendersBuiltInsThenOwnPresetsAccessibly()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString error;
    QVERIFY2(loadCustomizePage(view, model, &error), qPrintable(error));
    for (const QSize size : {QSize(720, 900), QSize(1080, 900)}) {
        view.resize(size);
        view.show();
        QTest::qWait(50);
        QQuickItem *root = view.rootObject() != nullptr
            ? qobject_cast<QQuickItem *>(view.rootObject()) : nullptr;
        QVERIFY(root != nullptr);

        // The WYSIWYG editor is gone: no canvas, palette, outline or panes.
        QVERIFY(item(root, "customizeOutputCanvas") == nullptr);
        QVERIFY(item(root, "customizePalette") == nullptr);
        QVERIFY(item(root, "customizeProperties") == nullptr);

        auto *fixture = item(root, "customizeProfileCard_fixture");
        auto *alternate = item(root, "customizeProfileCard_alternate");
        auto *mac = item(root, "customizeProfileCard_macos-inspired");
        auto *mine = item(root, "customizeProfileCard_user-mine");
        QVERIFY(fixture != nullptr && alternate != nullptr && mac != nullptr && mine != nullptr);
        QVERIFY(fixture->isVisible() && mine->isVisible());
        QVERIFY2(accessibleName(fixture).contains(QStringLiteral("layout preset")),
                 qPrintable(accessibleName(fixture)));
        QVERIFY(accessibleChecked(fixture));
        QVERIFY(!accessibleChecked(alternate));
        QVERIFY2(accessibleName(alternate).contains(QStringLiteral("modified")),
                 qPrintable(accessibleName(alternate)));

        // Built-ins come first: My presets is laid out below them.
        auto *builtIns = item(root, "customizeBuiltInPresets");
        auto *own = item(root, "customizeOwnPresets");
        QVERIFY(builtIns != nullptr && own != nullptr);
        QVERIFY(builtIns->mapToScene(QPointF()).y() < own->mapToScene(QPointF()).y());
        QVERIFY(item(builtIns, "customizeProfileCard_user-mine") == nullptr);
        QVERIFY(item(own, "customizeProfileCard_user-mine") != nullptr);

        // Badges are words, never colour alone.
        QVERIFY(item(root, "customizePresetModifiedBadge_alternate")->isVisible());
        QVERIFY(!item(root, "customizePresetModifiedBadge_fixture")->isVisible());
        QVERIFY(item(root, "customizePresetDefaultBadge_macos-inspired")->isVisible());

        // Each kind offers only its own actions.
        for (const char *name : {"customizePresetRename_user-mine",
                                 "customizePresetDuplicate_user-mine",
                                 "customizePresetDelete_user-mine",
                                 "customizePresetRestore_alternate",
                                 "customizePresetSaveAs_alternate"}) {
            QVERIFY2(item(root, name) != nullptr && item(root, name)->isVisible(), name);
        }
        for (const char *name : {"customizePresetRename_fixture",
                                 "customizePresetDelete_fixture",
                                 "customizePresetRestore_fixture",
                                 "customizePresetDelete_alternate",
                                 "customizePresetRestore_user-mine"}) {
            QVERIFY2(item(root, name) == nullptr || !item(root, name)->isVisible(), name);
        }
        QVERIFY2(accessibleName(item(root, "customizePresetDelete_user-mine"))
                     .contains(QStringLiteral("Mine")),
                 qPrintable(accessibleName(item(root, "customizePresetDelete_user-mine"))));
        QVERIFY(item(root, "customizeEditHint")->isVisible());
        QVERIFY(item(root, "customizeSaveCurrentPreset")->isVisible());
    }

    // With no own presets the section explains how to make one.
    model.setOwnPreset(false);
    QTRY_VERIFY(item(qobject_cast<QQuickItem *>(view.rootObject()),
                     "customizeProfileCard_user-mine") == nullptr);
}

void CustomizePageTests::cardsSwitchByPointerAndKeyboard()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString error;
    QVERIFY2(loadCustomizePage(view, model, &error), qPrintable(error));
    view.resize(1080, 900);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *root = qobject_cast<QQuickItem *>(view.rootObject());

    click(view, item(root, "customizeProfileCard_alternate"));
    QTRY_COMPARE(model.calls.size(), 1);
    QCOMPARE(model.calls.last(),
             (QVariantList{QStringLiteral("activatePreset"), QStringLiteral("alternate")}));

    auto *mac = item(root, "customizeProfileCard_macos-inspired");
    mac->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(mac->hasActiveFocus());
    QTest::keyClick(&view, Qt::Key_Space);
    QTRY_COMPARE(model.calls.size(), 2);
    QCOMPARE(model.calls.last().at(1).toString(), QStringLiteral("macos-inspired"));
    QTest::keyClick(&view, Qt::Key_Return);
    QTRY_COMPARE(model.calls.size(), 3);
    QCOMPARE(model.calls.last().at(1).toString(), QStringLiteral("macos-inspired"));

    // The page's first focus target is the first built-in card.
    QCOMPARE(root->property("firstFocusTarget").value<QQuickItem *>(),
             item(root, "customizeProfileCard_fixture"));
}

void CustomizePageTests::saveCurrentLayoutValidatesTheName()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString error;
    QVERIFY2(loadCustomizePage(view, model, &error), qPrintable(error));
    view.resize(1080, 900);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *root = qobject_cast<QQuickItem *>(view.rootObject());

    click(view, item(root, "customizeSaveCurrentPreset"));
    QTRY_VERIFY(opened(view, "customizePresetNameDialog"));
    auto *field = windowItem(view, "customizePresetNameField");
    auto *save = windowItem(view, "customizePresetNameSave");
    auto *message = windowItem(view, "customizePresetNameError");
    QVERIFY(field != nullptr && save != nullptr && message != nullptr);
    QTRY_VERIFY(field->hasActiveFocus());
    QCOMPARE(field->property("text").toString(), QString());
    QVERIFY(!save->isEnabled());
    QVERIFY(!message->isVisible());

    field->setProperty("text", QStringLiteral("fixture"));
    QTRY_VERIFY(message->isVisible());
    QVERIFY(!save->isEnabled());
    QVERIFY2(accessibleName(message).contains(QStringLiteral("exists")),
             qPrintable(accessibleName(message)));
    QVERIFY(model.calls.isEmpty());

    field->setProperty("text", QStringLiteral("Evening"));
    QTRY_VERIFY(save->isEnabled());
    click(view, save);
    QTRY_COMPARE(model.calls.size(), 1);
    QCOMPARE(model.calls.last(),
             (QVariantList{QStringLiteral("savePresetAs"), QStringLiteral("fixture"),
                           QStringLiteral("Evening")}));
    QTRY_VERIFY(!opened(view, "customizePresetNameDialog"));

    // A store failure keeps the dialog open with the model's reason.
    model.admitStoreActions = false;
    model.setMessages({}, QStringLiteral("disk full"));
    click(view, item(root, "customizeSaveCurrentPreset"));
    QTRY_VERIFY(opened(view, "customizePresetNameDialog"));
    field->setProperty("text", QStringLiteral("Night"));
    QTRY_VERIFY(save->isEnabled());
    click(view, save);
    QTRY_COMPARE(model.calls.size(), 2);
    QVERIFY(opened(view, "customizePresetNameDialog"));
    QTRY_VERIFY(accessibleName(message).contains(QStringLiteral("disk full")));
    click(view, windowItem(view, "customizePresetNameCancel"));
    QTRY_VERIFY(!opened(view, "customizePresetNameDialog"));
}

void CustomizePageTests::ownPresetsRenameDuplicateAndDeleteAfterConfirmation()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString error;
    QVERIFY2(loadCustomizePage(view, model, &error), qPrintable(error));
    view.resize(1080, 900);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *root = qobject_cast<QQuickItem *>(view.rootObject());

    click(view, item(root, "customizePresetRename_user-mine"));
    QTRY_VERIFY(opened(view, "customizePresetNameDialog"));
    auto *field = windowItem(view, "customizePresetNameField");
    QCOMPARE(field->property("text").toString(), QStringLiteral("Mine"));
    field->setProperty("text", QStringLiteral("Mine, renamed"));
    click(view, windowItem(view, "customizePresetNameSave"));
    QTRY_COMPARE(model.calls.size(), 1);
    QCOMPARE(model.calls.last(),
             (QVariantList{QStringLiteral("renamePreset"), QStringLiteral("user-mine"),
                           QStringLiteral("Mine, renamed")}));
    QTRY_VERIFY(!opened(view, "customizePresetNameDialog"));

    click(view, item(root, "customizePresetDuplicate_user-mine"));
    QTRY_COMPARE(model.calls.size(), 2);
    QCOMPARE(model.calls.last(),
             (QVariantList{QStringLiteral("duplicatePreset"), QStringLiteral("user-mine")}));

    // Delete asks first; Cancel deletes nothing.
    click(view, item(root, "customizePresetDelete_user-mine"));
    QTRY_VERIFY(opened(view, "customizePresetConfirmDialog"));
    QVERIFY(accessibleName(windowItem(view, "customizePresetConfirmText"))
                .contains(QStringLiteral("Mine")));
    click(view, windowItem(view, "customizePresetConfirmCancel"));
    QTRY_VERIFY(!opened(view, "customizePresetConfirmDialog"));
    QCOMPARE(model.calls.size(), 2);

    // Deleting the preset in use says the desktop moves to the default first.
    model.setActivePreset(QStringLiteral("user-mine"));
    click(view, item(root, "customizePresetDelete_user-mine"));
    QTRY_VERIFY(opened(view, "customizePresetConfirmDialog"));
    const QString warning = accessibleName(windowItem(view, "customizePresetConfirmText"));
    QVERIFY2(warning.contains(QStringLiteral("current layout"))
                 && warning.contains(QStringLiteral("Mac")),
             qPrintable(warning));
    click(view, windowItem(view, "customizePresetConfirmAccept"));
    QTRY_COMPARE(model.calls.size(), 3);
    QCOMPARE(model.calls.last(),
             (QVariantList{QStringLiteral("deletePreset"), QStringLiteral("user-mine")}));
    QTRY_VERIFY(!opened(view, "customizePresetConfirmDialog"));
}

void CustomizePageTests::editedBuiltInRestoresAfterConfirmationOrSavesAsNew()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString error;
    QVERIFY2(loadCustomizePage(view, model, &error), qPrintable(error));
    view.resize(1080, 900);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *root = qobject_cast<QQuickItem *>(view.rootObject());

    click(view, item(root, "customizePresetRestore_alternate"));
    QTRY_VERIFY(opened(view, "customizePresetConfirmDialog"));
    QVERIFY(accessibleName(windowItem(view, "customizePresetConfirmText"))
                .contains(QStringLiteral("Alternate")));
    QVERIFY(model.calls.isEmpty());
    click(view, windowItem(view, "customizePresetConfirmAccept"));
    QTRY_COMPARE(model.calls.size(), 1);
    QCOMPARE(model.calls.last(),
             (QVariantList{QStringLiteral("restorePreset"), QStringLiteral("alternate")}));
    QTRY_VERIFY(!opened(view, "customizePresetConfirmDialog"));

    click(view, item(root, "customizePresetSaveAs_alternate"));
    QTRY_VERIFY(opened(view, "customizePresetNameDialog"));
    QCOMPARE(windowItem(view, "customizePresetNameField")->property("text").toString(),
             QStringLiteral("Alternate (edited)"));
    click(view, windowItem(view, "customizePresetNameSave"));
    QTRY_COMPARE(model.calls.size(), 2);
    QCOMPARE(model.calls.last(),
             (QVariantList{QStringLiteral("savePresetAs"), QStringLiteral("alternate"),
                           QStringLiteral("Alternate (edited)")}));
}

void CustomizePageTests::busyAndUnavailableStatesStayTruthful()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString error;
    QVERIFY2(loadCustomizePage(view, model, &error), qPrintable(error));
    view.resize(1080, 900);
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    auto *root = qobject_cast<QQuickItem *>(view.rootObject());

    model.setBusy(true);
    QTRY_VERIFY(!item(root, "customizeProfileCard_alternate")->isEnabled());
    QVERIFY(!item(root, "customizePresetDelete_user-mine")->isEnabled());
    QVERIFY(!item(root, "customizeSaveCurrentPreset")->isEnabled());
    model.setBusy(false);
    QTRY_VERIFY(item(root, "customizeProfileCard_alternate")->isEnabled());

    model.setMessages(QStringLiteral("Switched to Alternate."), {});
    QTRY_VERIFY(item(root, "customizeNotice")->isVisible());
    QVERIFY(!item(root, "customizeError")->isVisible());
    model.setMessages({}, QStringLiteral("Settings refused the layout change: busy"));
    QTRY_VERIFY(item(root, "customizeError")->isVisible());
    QVERIFY(!item(root, "customizeNotice")->isVisible());

    model.setUnavailable(true);
    auto *notice = item(root, "customizeUnavailableNotice");
    QTRY_VERIFY(notice != nullptr && notice->isVisible());
    QVERIFY(!item(root, "customizeBuiltInPresets")->isVisible());
    QVERIFY(!item(root, "customizeSaveCurrentPreset")->isVisible());
    // The global delay is not a layout property and stays reachable.
    QVERIFY(item(root, "customizePanelHideDelaySlider")->isVisible());
}

void CustomizePageTests::panelHideDelayCommitsFinalPointerAndKeyboardIntent()
{
    StubCustomizeSettingsModel model;
    QQuickView view;
    QString error;
    QVERIFY2(loadCustomizePage(view, model, &error), qPrintable(error));
    // Tall enough that the whole page, the delay slider included, is on screen.
    view.resize(1080, 1600);
    view.show();
    auto *root = qobject_cast<QQuickItem *>(view.rootObject());
    auto *slider = item(root, "customizePanelHideDelaySlider");
    auto *status = item(root, "customizePanelHideDelayStatus");
    QVERIFY(slider != nullptr);
    QVERIFY(status != nullptr);
    QTRY_VERIFY(slider->isVisible());
    QVERIFY(slider->isEnabled());
    QCOMPARE(slider->property("value").toInt(), 250);

    slider->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(slider->hasActiveFocus());
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    QCoreApplication::sendEvent(&view, &press);
    QKeyEvent repeatPress(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier,
                          QString(), true);
    QCoreApplication::sendEvent(&view, &repeatPress);
    QKeyEvent repeatRelease(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier,
                            QString(), true);
    QCoreApplication::sendEvent(&view, &repeatRelease);
    QCoreApplication::processEvents();
    QCOMPARE(model.delaySetCount, 0);
    QCOMPARE(slider->property("value").toInt(), 350);
    QKeyEvent release(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);
    QCoreApplication::sendEvent(&view, &release);
    QTRY_COMPARE(model.delaySetCount, 1);
    QCOMPARE(model.lastDelayMs, 350);
    QTRY_COMPARE(slider->property("value").toInt(), 250);

    const QPoint start = slider->mapToScene(
        QPointF(slider->width() * 0.05, slider->height() / 2)).toPoint();
    const QPoint finish = slider->mapToScene(
        QPointF(slider->width() * 0.78, slider->height() / 2)).toPoint();
    QTest::mousePress(&view, Qt::LeftButton, Qt::NoModifier, start);
    QTest::mouseMove(&view, finish, 20);
    QTest::mouseRelease(&view, Qt::LeftButton, Qt::NoModifier, finish);
    QTRY_COMPARE(model.delaySetCount, 2);
    QVERIFY(model.lastDelayMs >= 3500);
    QVERIFY(model.lastDelayMs <= 4300);
    QTRY_COMPARE(slider->property("value").toInt(), 250);

    model.setDelayState(false, false, false, 250,
                        QStringLiteral("Saved delay unavailable"));
    QTRY_VERIFY(!slider->isEnabled());
    QCOMPARE(status->property("text").toString(),
             QStringLiteral("Saved delay unavailable"));
}

QTEST_MAIN(CustomizePageTests)
#include "tst_customize_page.moc"
