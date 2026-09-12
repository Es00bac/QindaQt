// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_config_port.h>
#include <qindaqt/apps/settings_input/keyboard_layout_port.h>
#include <qindaqt/apps/settings_input/keyboard_layouts_model.h>
#include <qindaqt/apps/settings_input/keyboard_settings_model.h>
#include <qindaqt/apps/settings_input/pointer_device_port.h>
#include <qindaqt/apps/settings_input/pointer_devices_model.h>
#include <qindaqt/apps/settings_input/shortcut_port.h>
#include <qindaqt/apps/settings_input/shortcuts_model.h>
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/themes/theme_loader.h>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

#include <QQuickWindow>
#include <QSet>
#include <QSignalSpy>
#include <QTest>

#include "support/fake_ports.h"

namespace QindaQt::Apps::SettingsInput {

using namespace QindaQt::Tests;

namespace {
// Messages Qt reports while the page is created; they carry the real reason
// when creation fails and are attached to the failure text below.
QStringList &creationMessages()
{
    static QStringList messages;
    return messages;
}

// List delegates hang off their view's content item without being its QObject
// children, so the search walks the visual tree as well as the object tree.
QObject *findInTree(QObject *root, const QString &objectName)
{
    QList<QObject *> pending{root};
    QSet<QObject *> seen;
    while (!pending.isEmpty()) {
        QObject *node = pending.takeFirst();
        if (node == nullptr || seen.contains(node)) {
            continue;
        }
        seen.insert(node);
        if (node->objectName() == objectName) {
            return node;
        }
        pending.append(node->children());
        if (auto *item = qobject_cast<QQuickItem *>(node)) {
            const QList<QQuickItem *> childItems = item->childItems();
            for (QQuickItem *child : childItems) {
                pending.append(child);
            }
        }
    }
    return nullptr;
}

ShortcutAction shellAction(const QString &actionUnique,
                           const QString &actionFriendly,
                           const QList<QKeySequence> &active)
{
    ShortcutAction action;
    action.componentUnique = QStringLiteral("qindaqt-shell");
    action.componentFriendly = QStringLiteral("QindaQt Shell");
    action.actionUnique = actionUnique;
    action.actionFriendly = actionFriendly;
    action.active = active;
    action.defaults = active;
    return action;
}
} // namespace

// The facade the page binds to, built on real models over fake ports: the
// rows exercise the exact QML the route ships, with no authority reachable.
class TestInputFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *pointerDevices READ pointerDevices CONSTANT)
    Q_PROPERTY(QObject *keyboard READ keyboard CONSTANT)
    Q_PROPERTY(QObject *layouts READ layouts CONSTANT)
    Q_PROPERTY(QObject *shortcuts READ shortcuts CONSTANT)

public:
    explicit TestInputFacade(QObject *parent = nullptr)
        : QObject(parent),
          pointerModel(m_pointerPort),
          keyboardModel(m_configPort),
          layoutsModel(m_layoutPort),
          shortcutsModel(m_shortcutPort) {}

    FakePointerPort m_pointerPort;
    FakeKeyboardConfigPort m_configPort;
    FakeLayoutPort m_layoutPort;
    FakeShortcutPort m_shortcutPort;
    PointerDevicesModel pointerModel;
    KeyboardSettingsModel keyboardModel;
    KeyboardLayoutsModel layoutsModel;
    ShortcutsModel shortcutsModel;

    QObject *pointerDevices() { return &pointerModel; }
    QObject *keyboard() { return &keyboardModel; }
    QObject *layouts() { return &layoutsModel; }
    QObject *shortcuts() { return &shortcutsModel; }
};

class InputPageTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void init(); // per-test fresh facade, engine and window
    void cleanup();

    void capabilityHidingHidesUnsupportedRows();
    void editorsSeatInsideTheirFormRows();
    void conflictCaptureNamesTheConflictingAction();
    void captureFlowEscapeCancelsAndBackspaceClears();
    void keyboardNavigationReachesTabsAndControls();
    void degradedSectionsWhenAuthorityAbsent();

private:
    [[nodiscard]] QObject *findObject(const QString &objectName) const;
    [[nodiscard]] bool isShown(const QString &objectName) const;
    [[nodiscard]] bool selectDestination(const QString &destination);

    std::unique_ptr<TestInputFacade> facade;
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QQuickWindow> window;
    QObject *page = nullptr;
};

void InputPageTest::init() {
    facade = std::make_unique<TestInputFacade>();
    engine = std::make_unique<QQmlEngine>();
    engine->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
    // Controls read every metric and color from Tokens; publish the shipped
    // dark theme so rows lay out as they do in the application.
    QString facadeError;
    auto *tokens = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
        *engine, &facadeError);
    QVERIFY2(tokens != nullptr, qPrintable(facadeError));
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QString publishError;
    QVERIFY2(tokens->publish(theme.theme, {}, &publishError),
             qPrintable(publishError));
    // The module directory carries a qmldir, so the implicit directory
    // import is off there; load through the module import instead.
    QQmlComponent component(engine.get());
    component.setData(
        QByteArray("import QtQuick\n"
                   "import QindaQt.SettingsApp.Input\n"
                   "InputPage { objectName: \"inputPage\" }\n"),
        QUrl(QStringLiteral("qindaqt-test://input-page")));
    // Module imports can finish on a later event-loop cycle, and creating a
    // component that is still loading returns no object.
    QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading,
                             10000);
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    // AGENT-NOTE: Creation messages are captured rather than printed, so a
    // failed creation reports Qt's real reason in the assertion below.
    creationMessages().clear();
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &,
                              const QString &message) {
        creationMessages().append(message);
    });
    page = component.createWithInitialProperties(
        {{QStringLiteral("inputSettings"), QVariant::fromValue(
              static_cast<QObject *>(facade.get()))}});
    qInstallMessageHandler(nullptr);
    auto *pageItem = qobject_cast<QQuickItem *>(page);
    QVERIFY2(pageItem != nullptr,
             qPrintable(component.errorString() +
                        creationMessages().join(QLatin1Char('\n'))));
    window = std::make_unique<QQuickWindow>();
    window->resize(960, 680);
    pageItem->setParentItem(window->contentItem());
    pageItem->setSize(QSizeF(960, 680));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
}

void InputPageTest::cleanup() {
    window.reset();
    engine.reset();
    facade.reset();
    page = nullptr;
}

QObject *InputPageTest::findObject(const QString &objectName) const {
    return page != nullptr ? findInTree(page, objectName) : nullptr;
}

bool InputPageTest::isShown(const QString &objectName) const {
    const QObject *object = findObject(objectName);
    return object != nullptr && object->property("visible").toBool();
}

bool InputPageTest::selectDestination(const QString &destination) {
    // The page function takes an untyped JavaScript argument, which the meta
    // object exposes as QVariant.
    const bool invoked = QMetaObject::invokeMethod(
        page, "selectDestination", Q_ARG(QVariant, QVariant(destination)));
    QTest::qWait(0); // runs the deferred first-focus handoff
    return invoked &&
           findObject(QStringLiteral("inputDestinationPage_") + destination) !=
               nullptr;
}

void InputPageTest::capabilityHidingHidesUnsupportedRows() {
    // One mouse: speed and profile supported; natural scrolling and every
    // touchpad row are hidden rather than shown disabled (ADR-0134).
    PointerDeviceSnapshot mouse;
    mouse.deviceId = QStringLiteral("event5");
    mouse.name = QStringLiteral("Fake Mouse");
    mouse.pointer = true;
    mouse.properties = QVariantMap{
        {QStringLiteral("pointerAcceleration"), 0.0},
        {QStringLiteral("supportsPointerAcceleration"), true},
        {QStringLiteral("supportsPointerAccelerationProfileFlat"), true},
        {QStringLiteral("supportsPointerAccelerationProfileAdaptive"), true},
        {QStringLiteral("supportsNaturalScroll"), false},
        {QStringLiteral("naturalScroll"), false},
    };
    facade->m_pointerPort.scripted.append(mouse);
    facade->pointerModel.refresh();

    QTRY_VERIFY(isShown(QStringLiteral("inputPointerSpeedRow")));
    QVERIFY(findObject(QStringLiteral("inputPointerSpeedSlider")) != nullptr);
    QVERIFY(!isShown(QStringLiteral("inputPointerNaturalScrollRow")));
    QVERIFY(!isShown(QStringLiteral("inputTouchpadHeader")));
    QVERIFY(!isShown(QStringLiteral("inputTouchpadTapToClickRow")));
}

void InputPageTest::editorsSeatInsideTheirFormRows() {
    // An editor assigned inline (`editor: Slider {}`) is created without a
    // parent and would never render: the row shows only its label text while
    // object-existence checks keep passing. FormRow must seat every assigned
    // editor in the row's editor host so the control is visible and
    // interactive (regression for the Input route blank-form defect).
    PointerDeviceSnapshot mouse;
    mouse.deviceId = QStringLiteral("event5");
    mouse.name = QStringLiteral("Fake Mouse");
    mouse.pointer = true;
    mouse.properties = QVariantMap{
        {QStringLiteral("pointerAcceleration"), 0.0},
        {QStringLiteral("supportsPointerAcceleration"), true},
        {QStringLiteral("supportsNaturalScroll"), true},
        {QStringLiteral("naturalScroll"), false},
    };
    facade->m_pointerPort.scripted.append(mouse);
    facade->pointerModel.refresh();

    auto *row = qobject_cast<QQuickItem *>(
        findObject(QStringLiteral("inputPointerSpeedRow")));
    QTRY_VERIFY(row != nullptr);
    auto *naturalScrollRow = qobject_cast<QQuickItem *>(
        findObject(QStringLiteral("inputPointerNaturalScrollRow")));
    QVERIFY(naturalScrollRow != nullptr);
    auto *slider = qobject_cast<QQuickItem *>(
        findObject(QStringLiteral("inputPointerSpeedSlider")));
    QVERIFY(slider != nullptr);
    auto *naturalScroll = qobject_cast<QQuickItem *>(
        findObject(QStringLiteral("inputPointerNaturalScrollSwitch")));
    QVERIFY(naturalScroll != nullptr);

    const auto seated = [](const QQuickItem *editor, const QQuickItem *owner) {
        bool inHost = false;
        bool underRow = false;
        for (const QQuickItem *ancestor = editor->parentItem();
             ancestor != nullptr; ancestor = ancestor->parentItem()) {
            inHost = inHost ||
                     ancestor->objectName() == QLatin1String("formRowEditorHost");
            underRow = underRow || ancestor == owner;
        }
        return inHost && underRow;
    };
    QVERIFY(seated(slider, row));
    QVERIFY(slider->width() > 0);
    QVERIFY(seated(naturalScroll, naturalScrollRow));
}

void InputPageTest::conflictCaptureNamesTheConflictingAction() {
    auto &actions = facade->m_shortcutPort.mutableScripted();
    actions.append(shellAction(QStringLiteral("qindaqt_reveal_panels"),
                               QStringLiteral("Reveal QindaQt panels"),
                               {QKeySequence(Qt::META | Qt::Key_Space)}));
    actions.append(shellAction(QStringLiteral("qindaqt_open_launcher"),
                               QStringLiteral("Open launcher"), {}));
    QVERIFY(selectDestination(QStringLiteral("shortcuts")));

    // The list takes its rows' height inside the page's scroll view.
    auto *list = qobject_cast<QQuickItem *>(
        findObject(QStringLiteral("inputShortcutsList")));
    QVERIFY(list != nullptr);
    QTRY_VERIFY(list->height() > 0);

    QObject *capture = findObject(QStringLiteral("inputShortcutCapture_1"));
    QVERIFY(capture != nullptr);
    QVERIFY(QMetaObject::invokeMethod(capture, "beginCapture"));
    QTest::keyClick(window.get(), Qt::Key_Space, Qt::MetaModifier);
    // The chord belongs to the reveal action; the launcher row names it and
    // waits for an explicit decision.
    QTRY_VERIFY(isShown(QStringLiteral("inputShortcutConflict_1")));
    const QString text = findObject(QStringLiteral("inputShortcutConflict_1"))
                             ->property("text")
                             .toString();
    QVERIFY(text.contains(QStringLiteral("QindaQt Shell")));
    QVERIFY(text.contains(QStringLiteral("Reveal QindaQt panels")));
    const auto &assigned = facade->m_shortcutPort.assigned();
    QVERIFY(assigned.isEmpty());

    // Assign anyway releases the chord from its holder, then dispatches the
    // exact encoded chord for the launcher.
    QObject *assign =
        findObject(QStringLiteral("inputShortcutConflictAssign_1"));
    QVERIFY(assign != nullptr);
    QVERIFY(QMetaObject::invokeMethod(assign, "click"));
    const int metaSpace = int(Qt::META) | int(Qt::Key_Space);
    QCOMPARE(assigned.size(), 2);
    QCOMPARE(assigned.at(0),
             qMakePair(QStringLiteral("qindaqt_reveal_panels"), 0));
    QCOMPARE(assigned.at(1),
             qMakePair(QStringLiteral("qindaqt_open_launcher"), metaSpace));

    // Capturing the chord the launcher now holds is not a conflict and
    // assigns straight away.
    QTest::qWait(0); // the refresh replaced the row delegates
    capture = findObject(QStringLiteral("inputShortcutCapture_1"));
    QVERIFY(capture != nullptr);
    QVERIFY(QMetaObject::invokeMethod(capture, "beginCapture"));
    QTest::keyClick(window.get(), Qt::Key_Space, Qt::MetaModifier);
    QCOMPARE(assigned.size(), 3);
    QCOMPARE(assigned.at(2),
             qMakePair(QStringLiteral("qindaqt_open_launcher"), metaSpace));
    QTest::qWait(0);
    QVERIFY(!isShown(QStringLiteral("inputShortcutConflict_1")));
}

void InputPageTest::captureFlowEscapeCancelsAndBackspaceClears() {
    // Drive the command capture control: Escape cancels, Backspace clears.
    QVERIFY(selectDestination(QStringLiteral("shortcuts")));
    QObject *capture = findObject(QStringLiteral("inputCommandCapture"));
    QVERIFY(capture != nullptr);
    QSignalSpy capturedSpy(capture, SIGNAL(captured(int)));
    QSignalSpy clearedSpy(capture, SIGNAL(cleared()));

    QVERIFY(QMetaObject::invokeMethod(capture, "beginCapture"));
    QCOMPARE(capture->property("capturing").toBool(), true);
    QTest::keyClick(window.get(), Qt::Key_Escape);
    QCOMPARE(capture->property("capturing").toBool(), false);
    QCOMPARE(capturedSpy.count(), 0);

    QVERIFY(QMetaObject::invokeMethod(capture, "beginCapture"));
    QTest::keyClick(window.get(), Qt::Key_J, Qt::MetaModifier);
    QCOMPARE(capturedSpy.count(), 1);
    QCOMPARE(capturedSpy.first().first().toInt(),
             int(Qt::META) | int(Qt::Key_J));

    QVERIFY(QMetaObject::invokeMethod(capture, "beginCapture"));
    QTest::keyClick(window.get(), Qt::Key_Backspace);
    QCOMPARE(capture->property("sequence").toInt(), 0);
    QCOMPARE(clearedSpy.count(), 1);
}

void InputPageTest::keyboardNavigationReachesTabsAndControls() {
    // The shortcuts destination builds its own section with a search field,
    // and the page's first-focus contract resolves to it.
    QVERIFY(selectDestination(QStringLiteral("shortcuts")));
    auto *search = qobject_cast<QQuickItem *>(
        findObject(QStringLiteral("inputShortcutsSearchField")));
    QVERIFY(search != nullptr);
    QCOMPARE(page->property("firstFocusTarget").value<QQuickItem *>(), search);

    // Tab from the destination bar walks into the section content.
    auto *tab = qobject_cast<QQuickItem *>(
        findObject(QStringLiteral("inputDestination_shortcuts")));
    QVERIFY(tab != nullptr);
    tab->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(tab->hasActiveFocus());
    QTest::keyClick(window.get(), Qt::Key_Tab);
    const QQuickItem *focused = window->activeFocusItem();
    QVERIFY(focused != nullptr);
    QVERIFY(focused != tab);
}

void InputPageTest::degradedSectionsWhenAuthorityAbsent() {
    facade->m_shortcutPort.authorityPresent = false;
    facade->m_pointerPort.authorityPresent = false;
    // Each destination builds its section when selected, and the section's
    // own refresh reports the unavailable authority.
    QVERIFY(selectDestination(QStringLiteral("shortcuts")));
    QTRY_VERIFY(isShown(QStringLiteral("inputShortcutsDegraded")));

    QVERIFY(selectDestination(QStringLiteral("pointers")));
    QTRY_VERIFY(isShown(QStringLiteral("inputPointerDegraded")));
}

} // namespace QindaQt::Apps::SettingsInput

QTEST_MAIN(QindaQt::Apps::SettingsInput::InputPageTest)
#include "tst_input_page.moc"
