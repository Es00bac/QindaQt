// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_input/keyboard_config_port.h>
#include <qindaqt/apps/settings_input/keyboard_layout_port.h>
#include <qindaqt/apps/settings_input/keyboard_layouts_model.h>
#include <qindaqt/apps/settings_input/keyboard_settings_model.h>
#include <qindaqt/apps/settings_input/pointer_device_port.h>
#include <qindaqt/apps/settings_input/pointer_devices_model.h>
#include <qindaqt/apps/settings_input/shortcut_port.h>
#include <qindaqt/apps/settings_input/shortcuts_model.h>

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>

#include <cstdio>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "support/fake_ports.h"

namespace QindaQt::Apps::SettingsInput {

using namespace QindaQt::Tests;

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
    void conflictCaptureNamesTheConflictingAction();
    void captureFlowEscapeCancelsAndBackspaceClears();
    void keyboardNavigationReachesTabsAndControls();
    void degradedSectionsWhenAuthorityAbsent();

private:
    QObject *findObject(const QString &objectName) const;
    QObject *loadPage();

    std::unique_ptr<TestInputFacade> facade;
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QQuickWindow> window;
    QObject *page = nullptr;
};

void InputPageTest::init() {
    facade = std::make_unique<TestInputFacade>();
    engine = std::make_unique<QQmlEngine>();
    engine->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
    // The module directory carries a qmldir, so the implicit directory
    // import is off there; load through the module import instead.
    QQmlComponent component(engine.get());
    component.setData(
        QByteArray("import QtQuick\n"
                   "import QindaQt.SettingsApp.Input\n"
                   "InputPage { objectName: \"inputPage\" }\n"),
        QUrl(QStringLiteral("qindaqt-test://input-page")));
    // AGENT-NOTE: QT_FATAL_WARNINGS aborts on QQmlComponent's generic
    // "Component is not ready" warning before this test can report the
    // component's real errorString. Mute the handler for the creation
    // call only; the errorString assertion below carries the truth.
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &,
                              const QString &) {});
    page = component.createWithInitialProperties(
        {{QStringLiteral("inputSettings"), QVariant::fromValue(
              static_cast<QObject *>(facade.get()))}});
    qInstallMessageHandler(nullptr);
    // Module imports can finish on the next event-loop cycle.
    if (component.status() == QQmlComponent::Loading) {
        QTRY_COMPARE_WITH_TIMEOUT(component.status(), QQmlComponent::Ready,
                                  10000);
    }
    std::fprintf(stderr, "PAGE-ERROR: status=%d error=%s\n",
                 int(component.status()),
                 qPrintable(component.errorString()));
    auto *pageItem = qobject_cast<QQuickItem *>(page);
    QVERIFY(pageItem != nullptr);
    window = std::make_unique<QQuickWindow>();
    window->resize(960, 680);
    pageItem->setParentItem(window->contentItem());
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
    if (page == nullptr) {
        return nullptr;
    }
    return page->findChild<QObject *>(objectName);
}

QObject *InputPageTest::loadPage() { return page; }

void InputPageTest::capabilityHidingHidesUnsupportedRows() {
    // One mouse: speed and profile supported; tap rows must not exist.
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

    QVERIFY(findObject(QStringLiteral("inputPointerSpeedRow")) != nullptr);
    QVERIFY(findObject(QStringLiteral("inputPointerNaturalScrollRow")) ==
            nullptr);
    // Touchpad-only rows are absent for a plain pointer.
    QVERIFY(findObject(QStringLiteral("inputTouchpadTapToClickRow")) ==
            nullptr);
    QVERIFY(findObject(QStringLiteral("inputPointerSpeedSlider")) !=
            nullptr);
}

void InputPageTest::conflictCaptureNamesTheConflictingAction() {
    facade->m_shortcutPort.mutableScripted().append([] {
        ShortcutAction action;
        action.componentUnique = QStringLiteral("qindaqt-shell");
        action.componentFriendly = QStringLiteral("QindaQt Shell");
        action.actionUnique = QStringLiteral("qindaqt_reveal_panels");
        action.actionFriendly = QStringLiteral("Reveal QindaQt panels");
        action.active = {QKeySequence(Qt::META | Qt::Key_Space)};
        action.defaults = action.active;
        return action;
    }());
    facade->shortcutsModel.refresh();
    QTest::qWait(0); // let the list delegates build

    auto *capture = findObject(QStringLiteral("inputShortcutCapture_0"));
    QVERIFY(capture != nullptr);
    QMetaObject::invokeMethod(capture, "beginCapture");
    QTest::keyClick(window.get(), Qt::Key_Space, Qt::MetaModifier);
    // The captured chord collides with the shell action; the row must name
    // it and wait for an explicit decision.
    auto *conflict = findObject(QStringLiteral("inputShortcutConflict_0"));
    QVERIFY(conflict != nullptr);
    QVERIFY(conflict->property("visible").toBool());
    const QString text = conflict->property("text").toString();
    QVERIFY(text.contains(QStringLiteral("QindaQt Shell")));
    QVERIFY(text.contains(QStringLiteral("Reveal QindaQt panels")));
    QVERIFY(facade->m_shortcutPort.assigned().isEmpty());

    // Assign anyway dispatches the exact encoded chord.
    auto *assign =
        findObject(QStringLiteral("inputShortcutConflictAssign_0"));
    QVERIFY(assign != nullptr);
    QMetaObject::invokeMethod(assign, "click");
    QCOMPARE(facade->m_shortcutPort.assigned().size(), 1);
    QCOMPARE(facade->m_shortcutPort.assigned().first().second,
             int(Qt::META) | int(Qt::Key_Space));
}

void InputPageTest::captureFlowEscapeCancelsAndBackspaceClears() {
    // Drive the capture control directly: Escape cancels, Backspace clears.
    auto *capture = findObject(QStringLiteral("inputCommandCapture"));
    QVERIFY(capture != nullptr);
    QSignalSpy capturedSpy(capture, SIGNAL(captured(int)));
    QSignalSpy clearedSpy(capture, SIGNAL(cleared()));

    QMetaObject::invokeMethod(capture, "beginCapture");
    QCOMPARE(capture->property("capturing").toBool(), true);
    QTest::keyClick(window.get(), Qt::Key_Escape);
    QCOMPARE(capture->property("capturing").toBool(), false);
    QCOMPARE(capturedSpy.count(), 0);

    QMetaObject::invokeMethod(capture, "beginCapture");
    QTest::keyClick(window.get(), Qt::Key_J, Qt::MetaModifier);
    QCOMPARE(capturedSpy.count(), 1);
    QCOMPARE(capturedSpy.first().first().toInt(),
             int(Qt::META) | int(Qt::Key_J));

    QMetaObject::invokeMethod(capture, "beginCapture");
    QTest::keyClick(window.get(), Qt::Key_Backspace);
    QCOMPARE(capture->property("sequence").toInt(), 0);
    QCOMPARE(clearedSpy.count(), 1);
}

void InputPageTest::keyboardNavigationReachesTabsAndControls() {
    // The shortcuts tab builds its own section with a search field.
    QMetaObject::invokeMethod(page, "selectDestination",
                              Q_ARG(QString, QStringLiteral("shortcuts")));
    QTest::qWait(0);
    QVERIFY(findObject(QStringLiteral("inputDestinationPage_shortcuts")) !=
            nullptr);
    auto *search = findObject(QStringLiteral("inputShortcutsSearchField"));
    QVERIFY(search != nullptr);
    // Tab from the destination bar walks into the section content.
    auto *tab =
        findObject(QStringLiteral("inputDestination_shortcuts"));
    QVERIFY(tab != nullptr);
    QMetaObject::invokeMethod(tab, "forceActiveFocus", Q_ARG(int, int(Qt::TabFocusReason)));
    QVERIFY(tab->property("activeFocus").toBool());
    QTest::keyClick(window.get(), Qt::Key_Tab);
    auto *focused = window->activeFocusItem();
    QVERIFY(focused != nullptr);
    QVERIFY(focused != qobject_cast<QQuickItem *>(tab));
    // The page's first-focus contract resolves to an admitted control.
    QVERIFY(findObject(QStringLiteral("inputDestinationList")) != nullptr);
}

void InputPageTest::degradedSectionsWhenAuthorityAbsent() {
    facade->m_shortcutPort.authorityPresent = false;
    facade->m_pointerPort.authorityPresent = false;
    // Navigate to the shortcuts section; the loader instantiates the page
    // and its own refresh reports the degraded truth.
    QMetaObject::invokeMethod(page, "selectDestination",
                              Q_ARG(QString, QStringLiteral("shortcuts")));
    QTest::qWait(0);
    auto *degraded = findObject(QStringLiteral("inputShortcutsDegraded"));
    QVERIFY(degraded != nullptr);
    QCOMPARE(degraded->property("visible").toBool(), true);

    QMetaObject::invokeMethod(page, "selectDestination",
                              Q_ARG(QString, QStringLiteral("pointers")));
    QTest::qWait(0);
    auto *pointerDegraded =
        findObject(QStringLiteral("inputPointerDegraded"));
    QVERIFY(pointerDegraded != nullptr);
    QCOMPARE(pointerDegraded->property("visible").toBool(), true);
}

} // namespace QindaQt::Apps::SettingsInput

QTEST_MAIN(QindaQt::Apps::SettingsInput::InputPageTest)
#include "tst_input_page.moc"
