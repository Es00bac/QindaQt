// SPDX-License-Identifier: GPL-3.0-or-later

#include "smart_lights_applet_controller.h"
#include "support/fakewiztransport.h"

#include <qindaqt/services/smart_lights_store/configuration_store.h>

#include <QtCore/QDir>
#include <QtCore/QTemporaryDir>
#include <QtQml/QQmlComponent>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlExtensionPlugin>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtGui/QGuiApplication>
#include <QtTest/QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_SmartLightsAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Shell::SmartLightsApplet;
using QindaQt::SmartLights::ConfigurationStore;
using QindaQt::Wiz::Testing::FakeWizClock;
using QindaQt::Wiz::Testing::FakeWizTransport;

namespace
{

const QString deviceMac = QStringLiteral("d8a011769356");
const QString deviceAddress = QStringLiteral("10.0.0.234");

[[nodiscard]] QVariantMap testTheme()
{
    return {{QStringLiteral("cornerRadius"), 8},
            {QStringLiteral("colors"),
             QVariantMap{{QStringLiteral("surfaceRaised"), QStringLiteral("#2c312e")},
                         {QStringLiteral("border"), QStringLiteral("#3c433f")},
                         {QStringLiteral("text"), QStringLiteral("#f2f1eb")},
                         {QStringLiteral("textMuted"), QStringLiteral("#a9afa9")},
                         {QStringLiteral("warning"), QStringLiteral("#e5a84b")}}}};
}

[[nodiscard]] QByteArray pilotReply()
{
    return QStringLiteral(
               R"({"method":"getPilot","result":{"mac":"%1","state":true,"dimming":50,"sceneId":0,"r":0,"g":0,"b":255,"c":0,"w":0,"rssi":-50}})")
        .arg(deviceMac)
        .toUtf8();
}

[[nodiscard]] QByteArray systemReply()
{
    return QStringLiteral(
               R"({"method":"getSystemConfig","result":{"mac":"%1","moduleName":"ESP25_SHRGB_01","fwVersion":"1.38.0"}})")
        .arg(deviceMac)
        .toUtf8();
}

[[nodiscard]] QByteArray modelReply()
{
    return QStringLiteral(
               R"({"method":"getModelConfig","result":{"mac":"%1","headTotal":1,"minDimLevel":1,"cctRange":[2200,2700,6500,6500]}})")
        .arg(deviceMac)
        .toUtf8();
}

[[nodiscard]] QList<QQuickItem *> itemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> matches;
    if (root->objectName() == name) {
        matches.append(root);
    }
    for (QQuickItem *child : root->childItems()) {
        matches.append(itemsNamed(child, name));
    }
    return matches;
}

} // namespace

class SmartLightsAppletQmlTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init();
    void cleanup();
    void rendersChipWithAccessibleSummary();
    void popupExposesDeviceControls();
    void switchDispatchesPowerIntent();
    void withoutControlGrantControlsAreInert();

private:
    void buildApplet(bool controlGranted = true);
    void introduceDevice();

    std::unique_ptr<QTemporaryDir> m_directory;
    std::unique_ptr<ConfigurationStore> m_store;
    std::unique_ptr<FakeWizTransport> m_transport;
    std::unique_ptr<FakeWizClock> m_clock;
    std::unique_ptr<QindaQt::Wiz::WizClient> m_client;
    std::unique_ptr<SmartLightsAppletController> m_controller;
    std::unique_ptr<QQmlEngine> m_engine;
    std::unique_ptr<QQmlComponent> m_component;
    std::unique_ptr<QObject> m_object;
    std::unique_ptr<QQuickWindow> m_window;
    QQuickItem *m_root = nullptr;

    // A popup declared as Popup.Window needs a shown parent window before it
    // can open, exactly as it does on a real panel surface.
    [[nodiscard]] QObject *openPopup();
};

void SmartLightsAppletQmlTests::init()
{
    m_directory = std::make_unique<QTemporaryDir>();
    m_store = std::make_unique<ConfigurationStore>(
        QDir(m_directory->path()).filePath(QStringLiteral("smart-lights.json")));
    m_transport = std::make_unique<FakeWizTransport>();
    m_clock = std::make_unique<FakeWizClock>();
    m_client = std::make_unique<QindaQt::Wiz::WizClient>(m_transport.get(), m_clock.get());
    m_client->setAutomaticPolling(false);
    m_client->start();
}

void SmartLightsAppletQmlTests::cleanup()
{
    m_object.reset();
    m_window.reset();
    m_component.reset();
    m_engine.reset();
    m_controller.reset();
    m_client.reset();
    m_clock.reset();
    m_transport.reset();
    m_store.reset();
    m_directory.reset();
    m_root = nullptr;
}

void SmartLightsAppletQmlTests::buildApplet(const bool controlGranted)
{
    m_controller = std::make_unique<SmartLightsAppletController>(
        m_client.get(), m_store.get(), true, controlGranted);

    m_engine = std::make_unique<QQmlEngine>();
    m_engine->addImportPath(QStringLiteral(QINDAQT_SMART_LIGHTS_QML_IMPORT_PATH));
    m_component = std::make_unique<QQmlComponent>(m_engine.get());
    m_component->setData(
        "import QtQuick\n"
        "import QindaQt.Shell.SmartLightsApplet 1.0\n"
        "SmartLightsApplet { access: controllerAccess; theme: appletTheme }\n",
        QUrl(QStringLiteral("qrc:/smartLightsAppletTest.qml")));

    m_engine->rootContext()->setContextProperty(QStringLiteral("controllerAccess"),
                                                m_controller.get());
    m_engine->rootContext()->setContextProperty(QStringLiteral("appletTheme"),
                                                testTheme());
    m_object.reset(m_component->create());
    QVERIFY2(m_object != nullptr, qPrintable(m_component->errorString()));
    m_root = qobject_cast<QQuickItem *>(m_object.get());
    QVERIFY(m_root != nullptr);

    m_window = std::make_unique<QQuickWindow>();
    m_window->setGeometry(0, 0, 480, 600);
    m_root->setParentItem(m_window->contentItem());
    m_window->show();
    QTRY_VERIFY(m_window->isExposed());
}

QObject *SmartLightsAppletQmlTests::openPopup()
{
    // Keyboard activation of the chip, which is also the accessibility path a
    // panel user without a pointer takes.
    auto *summary = m_root->findChild<QQuickItem *>(
        QStringLiteral("smartLightsAppletSummary"));
    if (summary == nullptr) {
        return nullptr;
    }
    summary->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    return m_root->findChild<QObject *>(QStringLiteral("smartLightsAppletPopup"));
}

void SmartLightsAppletQmlTests::introduceDevice()
{
    m_transport->deliver(deviceAddress, pilotReply());
    m_transport->deliver(deviceAddress, systemReply());
    m_transport->deliver(deviceAddress, modelReply());
    m_transport->unicasts.clear();
}

void SmartLightsAppletQmlTests::rendersChipWithAccessibleSummary()
{
    introduceDevice();
    buildApplet();

    const auto icons = itemsNamed(m_root, QStringLiteral("smartLightsAppletIcon"));
    QCOMPARE(icons.size(), 1);
    // A lit room shows the "on" glyph; the name resolves in Breeze too.
    QCOMPARE(icons.first()->property("name").toString(), QStringLiteral("brightness-high"));

    const auto summaries = itemsNamed(m_root, QStringLiteral("smartLightsAppletSummary"));
    QCOMPARE(summaries.size(), 1);
    QVERIFY(!m_controller->accessibleName().isEmpty());
    QVERIFY(m_controller->summaryLabel().contains(QStringLiteral("1")));
}

void SmartLightsAppletQmlTests::popupExposesDeviceControls()
{
    introduceDevice();
    buildApplet();

    QObject *popup = openPopup();
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());

    auto *content = popup->property("contentItem").value<QQuickItem *>();
    QVERIFY(content != nullptr);
    QTRY_COMPARE(itemsNamed(content, QStringLiteral("smartLightDeviceRow")).size(), 1);
    QCOMPARE(itemsNamed(content, QStringLiteral("smartLightPowerSwitch")).size(), 1);
    QVERIFY(!itemsNamed(content, QStringLiteral("smartLightsAppletAllOffButton")).isEmpty());
}

void SmartLightsAppletQmlTests::switchDispatchesPowerIntent()
{
    introduceDevice();
    buildApplet();

    QObject *popup = openPopup();
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    auto *content = popup->property("contentItem").value<QQuickItem *>();
    QVERIFY(content != nullptr);

    const auto switches = itemsNamed(content, QStringLiteral("smartLightPowerSwitch"));
    QTRY_COMPARE(switches.size(), 1);
    QVERIFY(switches.first()->property("enabled").toBool());
    // Activate the switch the way a keyboard user does. QQuickAbstractButton's
    // public toggle() only writes `checked`; it does not emit toggled(), so
    // driving it directly would test nothing.
    QVERIFY(switches.first()->property("checked").toBool());
    switches.first()->forceActiveFocus();
    QVERIFY(switches.first()->hasActiveFocus());
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QTRY_VERIFY(!switches.first()->property("checked").toBool());

    QTRY_COMPARE(m_transport->unicastCount("setPilot"), 1);
}

void SmartLightsAppletQmlTests::withoutControlGrantControlsAreInert()
{
    introduceDevice();
    buildApplet(false);

    QObject *popup = openPopup();
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());
    auto *content = popup->property("contentItem").value<QQuickItem *>();
    QVERIFY(content != nullptr);

    const auto switches = itemsNamed(content, QStringLiteral("smartLightPowerSwitch"));
    QTRY_COMPARE(switches.size(), 1);
    // The projection refuses control, so the rendered control is disabled and
    // nothing can leave the host.
    QVERIFY(!switches.first()->property("enabled").toBool());
    QCOMPARE(m_transport->unicastCount("setPilot"), 0);
}

QTEST_MAIN(SmartLightsAppletQmlTests)
#include "tst_smart_lights_applet_qml.moc"
