// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_appearance/appearance_qml_composition.h>
#include <qindaqt/apps/settings_input/keyboard_layouts_model.h>
#include <qindaqt/apps/settings_input/keyboard_settings_model.h>
#include <qindaqt/apps/settings_input/pointer_devices_model.h>
#include <qindaqt/apps/settings_input/shortcuts_model.h>
#include <qindaqt/apps/settings_input/tablet_devices_model.h>
#include <qindaqt/themes/theme_loader.h>

#include <QFile>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSet>
#include <QTest>

#include "support/fake_ports.h"
#include "support/fake_tablet_ports.h"

namespace QindaQt::Apps::SettingsInput {

using namespace QindaQt::Tests;

namespace {

QStringList &creationMessages() {
    static QStringList messages;
    return messages;
}

// Delegates hang off their view's content item without being its QObject
// children, so the search walks the visual tree as well as the object tree.
QObject *findInTree(QObject *root, const QString &objectName) {
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

} // namespace

// The facade the page binds to, built on real models over fake ports.
class TestTabletFacade final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *pointerDevices READ pointerDevices CONSTANT)
    Q_PROPERTY(QObject *keyboard READ keyboard CONSTANT)
    Q_PROPERTY(QObject *layouts READ layouts CONSTANT)
    Q_PROPERTY(QObject *shortcuts READ shortcuts CONSTANT)
    Q_PROPERTY(QObject *tabletDevices READ tabletDevices CONSTANT)

public:
    explicit TestTabletFacade(QObject *parent = nullptr)
        : QObject(parent), pointerModel(m_pointerPort),
          keyboardModel(m_configPort), layoutsModel(m_layoutPort),
          shortcutsModel(m_shortcutPort),
          tabletModel(tabletPort, outputs, &tabletStore) {}

    FakePointerPort m_pointerPort;
    FakeKeyboardConfigPort m_configPort;
    FakeLayoutPort m_layoutPort;
    FakeShortcutPort m_shortcutPort;
    FakeTabletPort tabletPort;
    FakeTabletOutputs outputs;
    FakeTabletMappingStore tabletStore;
    PointerDevicesModel pointerModel;
    KeyboardSettingsModel keyboardModel;
    KeyboardLayoutsModel layoutsModel;
    ShortcutsModel shortcutsModel;
    TabletDevicesModel tabletModel;

    QObject *pointerDevices() { return &pointerModel; }
    QObject *keyboard() { return &keyboardModel; }
    QObject *layouts() { return &layoutsModel; }
    QObject *shortcuts() { return &shortcutsModel; }
    QObject *tabletDevices() { return &tabletModel; }
};

class TabletPageTest final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void cleanup();

    void theRouteUnderTestIsTheBuildTreesQml();
    void theDestinationListOffersPenAndTablet();
    void everyControlRendersForAFullyCapableTablet();
    void unsupportedControlsAreHiddenNotDisabled();
    void noTabletShowsTheHonestEmptyState();
    void anUnreachableAuthorityShowsTheDegradedNotice();
    void theDeepLinkOpensTheDestinationWithTheDeviceSelected();
    void calibrationIsOfferedOnlyForATabletWithAScreenOfItsOwn();

private:
    void buildPage(const QString &destination = QStringLiteral("tablet"),
                   const QString &selection = QString());
    [[nodiscard]] QObject *findObject(const QString &objectName) const;
    [[nodiscard]] bool isShown(const QString &objectName) const;

    std::unique_ptr<TestTabletFacade> facade;
    std::unique_ptr<QQmlEngine> engine;
    std::unique_ptr<QQuickWindow> window;
    QObject *page = nullptr;
};

void TabletPageTest::buildPage(const QString &destination,
                               const QString &selection) {
    engine = std::make_unique<QQmlEngine>();
    engine->addImportPath(QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH));
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

    QQmlComponent component(engine.get());
    component.setData(QByteArray("import QtQuick\n"
                                 "import QindaQt.SettingsApp.Input\n"
                                 "InputPage { objectName: \"inputPage\" }\n"),
                      QUrl(QStringLiteral("qindaqt-test://tablet-page")));
    QTRY_VERIFY_WITH_TIMEOUT(component.status() != QQmlComponent::Loading,
                             10000);
    QVERIFY2(component.status() == QQmlComponent::Ready,
             qPrintable(component.errorString()));
    creationMessages().clear();
    qInstallMessageHandler([](QtMsgType, const QMessageLogContext &,
                              const QString &message) {
        creationMessages().append(message);
    });
    page = component.createWithInitialProperties({
        {QStringLiteral("inputSettings"),
         QVariant::fromValue(static_cast<QObject *>(facade.get()))},
        {QStringLiteral("initialDestination"), destination},
        {QStringLiteral("initialSelection"), selection},
    });
    qInstallMessageHandler(nullptr);
    auto *pageItem = qobject_cast<QQuickItem *>(page);
    QVERIFY2(pageItem != nullptr,
             qPrintable(component.errorString() +
                        creationMessages().join(QLatin1Char('\n'))));
    window = std::make_unique<QQuickWindow>();
    window->resize(960, 760);
    pageItem->setParentItem(window->contentItem());
    pageItem->setSize(QSizeF(960, 760));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window.get()));
}

void TabletPageTest::cleanup() {
    window.reset();
    engine.reset();
    facade.reset();
    page = nullptr;
}

QObject *TabletPageTest::findObject(const QString &objectName) const {
    return page != nullptr ? findInTree(page, objectName) : nullptr;
}

bool TabletPageTest::isShown(const QString &objectName) const {
    const auto *item = qobject_cast<QQuickItem *>(findObject(objectName));
    // AGENT-GUARD: Existence and `visible` alone pass for an unparented item
    // that never renders. A shown control has a parent chain to the window
    // and a positive size.
    return item != nullptr && item->isVisible() && item->width() > 0 &&
           item->height() > 0 && item->window() != nullptr;
}

void TabletPageTest::theRouteUnderTestIsTheBuildTreesQml() {
    // AGENT-GUARD: A targeted build root without the *_qmlplugin target
    // silently falls back to the INSTALLED qrc module, and a green row would
    // then prove nothing about this worktree. The compiled module's URLs are
    // `qrc:` either way, so the proof is the loaded plugin library: it must
    // be the one in THIS build root, not the one under the install prefix.
    facade = std::make_unique<TestTabletFacade>();
    buildPage();
    QObject *section = findObject(QStringLiteral("tabletDevicePicker"));
    QVERIFY(section != nullptr);
    const QUrl sectionUrl = qmlContext(section)->baseUrl();
    QVERIFY2(sectionUrl.toString().endsWith(
                 QStringLiteral("InputTabletSection.qml")),
             qPrintable(sectionUrl.toString()));

    QFile maps(QStringLiteral("/proc/self/maps"));
    QVERIFY(maps.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString mapped = QString::fromUtf8(maps.readAll());
    const QString expected =
        QStringLiteral(QINDAQT_BUILD_QML_IMPORT_PATH) +
        QStringLiteral("/QindaQt/SettingsApp/Input/"
                       "libqindaqt_settings_input_qmlplugin.so");
    QVERIFY2(mapped.contains(expected), qPrintable(expected));
    QVERIFY(!mapped.contains(QStringLiteral(
        "/usr/lib64/qt6/qml/QindaQt/SettingsApp/Input/")));
}

void TabletPageTest::theDestinationListOffersPenAndTablet() {
    facade = std::make_unique<TestTabletFacade>();
    buildPage(QString());
    QVERIFY(isShown(QStringLiteral("inputDestination_pointers")));
    QVERIFY(isShown(QStringLiteral("inputDestination_tablet")));
    QVERIFY(isShown(QStringLiteral("inputDestination_keyboard")));
    QVERIFY(isShown(QStringLiteral("inputDestination_shortcuts")));
    // With no deep link the route opens where it always did.
    QCOMPARE(page->property("currentDestination").toString(),
             QStringLiteral("pointers"));
}

void TabletPageTest::everyControlRendersForAFullyCapableTablet() {
    facade = std::make_unique<TestTabletFacade>();
    facade->tabletPort.scripted = {fakeWacomPen(), fakeWacomPad()};
    facade->outputs.scripted = {fakeLaptopPanel(), fakePenDisplayOutput()};
    buildPage();

    QVERIFY(isShown(QStringLiteral("tabletMapRow")));
    QVERIFY(isShown(QStringLiteral("tabletOutputRow")));
    QVERIFY(isShown(QStringLiteral("tabletMapDiagram")));
    QVERIFY(isShown(QStringLiteral("tabletAreaWholeScreen")));
    QVERIFY(isShown(QStringLiteral("tabletAreaKeepProportions")));
    QVERIFY(isShown(QStringLiteral("tabletRotationRow")));
    QVERIFY(isShown(QStringLiteral("tabletLeftHandedRow")));
    QVERIFY(isShown(QStringLiteral("tabletPenModeRow")));
    QVERIFY(isShown(QStringLiteral("tabletEnabledRow")));
    QVERIFY(isShown(QStringLiteral("tabletCalibrationStart")));
    QVERIFY(isShown(QStringLiteral("tabletPressureSection")));
    QVERIFY(isShown(QStringLiteral("tabletPressureThresholdRow")));
    QVERIFY(isShown(QStringLiteral("tabletPressureScribble")));
    QVERIFY(isShown(QStringLiteral("tabletPadSection")));
    QVERIFY(isShown(QStringLiteral("tabletResetDevice")));
    // The pad summary names the hardware KWin reported and nothing it did not.
    const QObject *padSummary = findObject(QStringLiteral("tabletPadSummary"));
    QVERIFY(padSummary != nullptr);
    const QString padText = padSummary->property("text").toString();
    QVERIFY(padText.contains(QStringLiteral("4")));
    QVERIFY(!padText.contains(QStringLiteral("strip")));
    // The calibration overlay is not up until the user asks for it.
    QVERIFY(findObject(QStringLiteral("tabletCalibrationOverlay")) == nullptr);
}

void TabletPageTest::unsupportedControlsAreHiddenNotDisabled() {
    facade = std::make_unique<TestTabletFacade>();
    auto pen = fakeWacomPen();
    pen.properties.insert(QStringLiteral("supportsCalibrationMatrix"), false);
    pen.properties.insert(QStringLiteral("supportsRotation"), false);
    pen.properties.insert(QStringLiteral("supportsLeftHanded"), false);
    pen.properties.insert(QStringLiteral("supportsPressureRange"), false);
    pen.properties.insert(QStringLiteral("supportsDisableEvents"), false);
    facade->tabletPort.scripted = {pen};
    buildPage();

    QVERIFY(!isShown(QStringLiteral("tabletRotationRow")));
    QVERIFY(!isShown(QStringLiteral("tabletLeftHandedRow")));
    QVERIFY(!isShown(QStringLiteral("tabletEnabledRow")));
    QVERIFY(!isShown(QStringLiteral("tabletCalibrationStart")));
    QVERIFY(!isShown(QStringLiteral("tabletPressureThresholdRow")));
    // A tablet with no pad shows no pad section at all.
    QVERIFY(!isShown(QStringLiteral("tabletPadSection")));
    // What the device does support still renders.
    QVERIFY(isShown(QStringLiteral("tabletMapRow")));
    QVERIFY(isShown(QStringLiteral("tabletPenModeRow")));
}

void TabletPageTest::noTabletShowsTheHonestEmptyState() {
    facade = std::make_unique<TestTabletFacade>();
    buildPage();
    QVERIFY(isShown(QStringLiteral("tabletNoDevices")));
    QVERIFY(!isShown(QStringLiteral("tabletDegraded")));
    QVERIFY(!isShown(QStringLiteral("tabletMapRow")));
}

void TabletPageTest::anUnreachableAuthorityShowsTheDegradedNotice() {
    facade = std::make_unique<TestTabletFacade>();
    facade->tabletPort.listError =
        QStringLiteral("Input authority org.kde.KWin is not reachable");
    buildPage();
    QVERIFY(isShown(QStringLiteral("tabletDegraded")));
    // Degraded is not the same claim as "no tablet is connected".
    QVERIFY(!isShown(QStringLiteral("tabletNoDevices")));
}

void TabletPageTest::theDeepLinkOpensTheDestinationWithTheDeviceSelected() {
    facade = std::make_unique<TestTabletFacade>();
    auto second = fakeWacomPen(QStringLiteral("event25"));
    second.name = QStringLiteral("Second Tablet Pen");
    second.productId = 935;
    facade->tabletPort.scripted = {fakeWacomPen(), second};
    buildPage(QStringLiteral("tablet"), QStringLiteral("1386:935:Second Tablet"));

    QCOMPARE(page->property("currentDestination").toString(),
             QStringLiteral("tablet"));
    QCOMPARE(facade->tabletModel.selection()->deviceGroupId(),
             QStringLiteral("1386:935:Second Tablet"));
    QVERIFY(isShown(QStringLiteral("tabletDevicePicker")));
    cleanup();

    // A destination the page does not know keeps the default rather than
    // leaving the route on nothing.
    facade = std::make_unique<TestTabletFacade>();
    buildPage(QStringLiteral("not-a-destination"));
    QCOMPARE(page->property("currentDestination").toString(),
             QStringLiteral("pointers"));
}

void TabletPageTest::calibrationIsOfferedOnlyForATabletWithAScreenOfItsOwn() {
    // AGENT-GUARD: calibration measures against a fixed surface. A tablet
    // that follows the active screen has none, so offering the wizard would
    // let the user calibrate against whichever screen happened to be active.
    facade = std::make_unique<TestTabletFacade>();
    auto following = fakeWacomPen();
    following.properties.insert(QStringLiteral("outputName"), QString());
    facade->tabletPort.scripted = {following};
    buildPage();
    const QObject *start = findObject(QStringLiteral("tabletCalibrationStart"));
    QVERIFY(start != nullptr);
    QVERIFY(!start->property("available").toBool());
    cleanup();

    // Mapped to a named screen: the wizard is available and names it.
    facade = std::make_unique<TestTabletFacade>();
    facade->outputs.scripted = {fakeLaptopPanel(), fakePenDisplayOutput()};
    facade->tabletPort.scripted = {fakeWacomPen()};
    buildPage();
    const QObject *mapped = findObject(QStringLiteral("tabletCalibrationStart"));
    QVERIFY(mapped != nullptr);
    QVERIFY(mapped->property("available").toBool());
    QVERIFY(mapped->property("accessibleDescription")
                .toString()
                .contains(QStringLiteral("HDMI-A-1")));
}

} // namespace QindaQt::Apps::SettingsInput

QTEST_MAIN(QindaQt::Apps::SettingsInput::TabletPageTest)
#include "tst_tablet_page.moc"
