// SPDX-License-Identifier: GPL-3.0-or-later
// Native visual/input proof over a real private-bus exporter. This executable
// never contacts the user's watcher and never injects system-wide input.
#include "status_notifier_fake_item_test_support.h"
#include "qindaqt/shell/global_menu/dbusmenu/dbusmenu_server.h"
#include "qindaqt/shell/status_notifier/applet/status_notifier_applet_controller.h"
#include "qindaqt/shell/status_notifier/applet/status_notifier_monitor_adapter.h"
#include "qindaqt/shell/status_notifier/watcher/status_notifier_watcher_service.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QScopeGuard>
#include <QtTest>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_StatusNotifierPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::StatusNotifier;
using namespace QindaQt::StatusNotifier::TestSupport;
using namespace QindaQt::StatusNotifierApplet;
namespace Menu = QindaQt::Shell::GlobalMenu;

namespace {
QQuickItem *findVisual(QQuickItem *root, const QString &objectName,
                       const QString &text = {})
{
    if (root->isVisible() && root->objectName() == objectName
        && (text.isEmpty() || root->property("text").toString() == text)) {
        return root;
    }
    for (auto *child : root->childItems()) {
        if (auto *found = findVisual(child, objectName, text)) return found;
    }
    return nullptr;
}

QQuickItem *visibleMenuItem(const QString &text, bool submenu = false)
{
    for (auto *window : QGuiApplication::allWindows()) {
        auto *quick = qobject_cast<QQuickWindow *>(window);
        if (quick && quick->isVisible()) {
            if (auto *item = findVisual(quick->contentItem(), submenu
                    ? QStringLiteral("statusNotifierSubmenuItem")
                    : QStringLiteral("statusNotifierMenuAction"), text)) return item;
        }
    }
    return nullptr;
}

void clickItem(QQuickItem *item, Qt::MouseButton button = Qt::LeftButton)
{
    QTest::mouseClick(item->window(), button, Qt::NoModifier,
                     item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
}

bool capture(QQuickWindow *window, const QString &path)
{
    const auto frame = window->grabWindow();
    return !frame.isNull() && frame.save(path);
}

bool insideScreen(QWindow *window)
{
    return window->screen() && window->screen()->geometry().contains(window->geometry());
}

Menu::Protocol::MenuTree fixtureTree()
{
    using Menu::Protocol::MenuItem;
    using Menu::Protocol::MenuItemKind;
    return {.ownerWindowId = QUuid::createUuid(), .epoch = QUuid::createUuid(), .revision = 1,
        .items = {
            MenuItem{.id = QStringLiteral("configuration"), .text = QStringLiteral("Configuration…")},
            MenuItem{.id = QStringLiteral("led"), .text = QStringLiteral("Battery-color mouse LED"),
                     .checkable = true},
            MenuItem{.id = QStringLiteral("tools"), .kind = MenuItemKind::Submenu,
                     .text = QStringLiteral("Tools"), .children = {
                MenuItem{.id = QStringLiteral("studio"), .text = QStringLiteral("Command Studio…")}}},
            MenuItem{.id = QStringLiteral("separator"), .kind = MenuItemKind::Separator},
            MenuItem{.id = QStringLiteral("disabled"), .text = QStringLiteral("Unavailable action"),
                     .enabled = false}}};
}
} // namespace

class TrayNativeProbe final : public QObject {
    Q_OBJECT
private slots:
    void exportedMenuOpensWindow_data();
    void exportedMenuOpensWindow();
};

void TrayNativeProbe::exportedMenuOpensWindow_data()
{
    QTest::addColumn<bool>("vertical");
    QTest::newRow("horizontal-bottom-right") << false;
    QTest::newRow("vertical-top-left") << true;
}

void TrayNativeProbe::exportedMenuOpensWindow()
{
    // AGENT-GUARD: A private Wayland runner must opt in. QApplication/QPA can
    // otherwise inherit the host display before this fixture creates its bus.
    QVERIFY(qEnvironmentVariable("QINDAQT_TRAY_NATIVE_PROBE") == QStringLiteral("1"));
    QCOMPARE(QGuiApplication::platformName(), QStringLiteral("wayland"));
    QFETCH(bool, vertical);
    const QString artifactRoot = qEnvironmentVariable("QINDAQT_TRAY_CAPTURE_DIR");
    QVERIFY(!artifactRoot.isEmpty());
    const QString directory = artifactRoot + QLatin1Char('/') + QString::fromLatin1(QTest::currentDataTag());
    QVERIFY(QDir().mkpath(directory));
    PrivateSessionBus privateBus;
    QString error;
    QVERIFY2(privateBus.start(&error), qPrintable(error));
    const QString connectionPrefix = QString::fromLatin1(QTest::currentDataTag());
    auto watcherConnection = connectToPrivateBus(privateBus.address(), connectionPrefix + "-watcher");
    auto monitorConnection = connectToPrivateBus(privateBus.address(), connectionPrefix + "-monitor");
    auto itemConnection = connectToPrivateBus(privateBus.address(), connectionPrefix + "-item");
    const auto disconnect = qScopeGuard([&] {
        QDBusConnection::disconnectFromBus(watcherConnection.name());
        QDBusConnection::disconnectFromBus(monitorConnection.name());
        QDBusConnection::disconnectFromBus(itemConnection.name());
    });
    StatusNotifierWatcherService watcher(watcherConnection);
    QVERIFY2(watcher.start(&error), qPrintable(error));
    FakeStatusNotifierItem item;
    item.id = QStringLiteral("org.qindaqt.NativeTrayFixture");
    item.title = QStringLiteral("Native tray fixture");
    item.iconPixmap = {FakeStatusNotifierItem::pixmap(22, 22, 0xda9c26ff)};
    item.menu = QDBusObjectPath(QStringLiteral("/MenuBar"));
    item.itemIsMenu = false;
    QVERIFY(registerFakeItem(itemConnection, QStringLiteral("/StatusNotifierItem"), &item));
    // Real shared v4 exporter, not an injected controller menu model: property,
    // layout, AboutToShow and Event traffic all cross the private bus.
    Menu::DbusMenu::DbusMenuServer menu;
    auto tree = fixtureTree();
    QVERIFY(menu.publish(tree));
    new FakePropertiesAdaptor(&menu);
    QVERIFY(itemConnection.registerObject(QStringLiteral("/MenuBar"), &menu,
        QDBusConnection::ExportAdaptors | QDBusConnection::ExportAllSlots
        | QDBusConnection::ExportAllProperties | QDBusConnection::ExportAllSignals));
    StatusNotifierMonitorAdapter adapter(monitorConnection, {}, 2'000);
    StatusNotifierAppletController controller(&adapter, true, true);
    adapter.start();
    auto registration = QDBusMessage::createMethodCall(QString::fromLatin1(kWatcherServiceName),
        QString::fromLatin1(kWatcherObjectPath), QString::fromLatin1(kWatcherInterfaceName),
        QStringLiteral("RegisterStatusNotifierItem"));
    registration << QStringLiteral("/StatusNotifierItem");
    auto pending = itemConnection.asyncCall(registration);
    QTRY_VERIFY_WITH_TIMEOUT(pending.isFinished(), 5'000);
    QCOMPARE(pending.reply().type(), QDBusMessage::ReplyMessage);
    QTRY_COMPARE_WITH_TIMEOUT(controller.itemCount(), 1, 5'000);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_TRAY_QML_IMPORT_PATH));
    QQmlComponent tokenRegistration(&engine);
    tokenRegistration.setData("import QtQuick\nimport QindaQt.Tokens 1.0\nQtObject {}", QUrl("inline:tokens.qml"));
    QTRY_VERIFY(!tokenRegistration.isLoading());
    std::unique_ptr<QObject> tokenObject(tokenRegistration.create());
    QVERIFY2(tokenObject, qPrintable(tokenRegistration.errorString()));
    auto *tokens = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>("QindaQt.Tokens", "Tokens");
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-light.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QVERIFY2(tokens && tokens->publish(theme.theme, {}, &error), qPrintable(error));
    QVERIFY(QindaQt::Shell::Icons::IconRuntime::install(engine,
        {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")}, {QStringLiteral("QindaQt")}));
    QQmlComponent component(&engine);
    component.loadFromModule("QindaQt.Shell.StatusNotifier", "StatusNotifierApplet");
    QTRY_VERIFY(!component.isLoading());
    std::unique_ptr<QObject> owned(component.createWithInitialProperties({
        {"access", QVariant::fromValue(&controller)}, {"theme", QVariantMap{}}, {"vertical", vertical}}));
    QVERIFY2(owned, qPrintable(component.errorString()));
    auto *applet = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(applet);
    // A fullscreen, non-focusing native host fixes applet anchors to actual
    // screen edges. It models panel focus refusal without claiming LayerShell
    // reservation or shell compositor integration coverage.
    QQuickWindow panel;
    panel.setFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus);
    panel.setColor(QColor("#eee9e5"));
    applet->setParentItem(panel.contentItem());
    panel.showFullScreen();
    QTRY_VERIFY(panel.isExposed());
    applet->setWidth(applet->implicitWidth());
    applet->setHeight(applet->implicitHeight());
    applet->setPosition(vertical ? QPointF(4, 4)
        : QPointF(panel.width() - applet->width() - 4, panel.height() - applet->height() - 4));
    QTRY_VERIFY(applet->width() > 0 && applet->height() > 0);
    auto *button = findVisual(applet, QStringLiteral("statusNotifierItemDelegate"));
    QVERIFY(button);
    QQmlComponent configurationComponent(&engine);
    configurationComponent.setData(R"(
import QtQuick
Window { width: 480; height: 240; color: "#f7f3ee"; title: "Fixture configuration"
    Text { anchors.centerIn: parent; text: "Configuration opened from the tray menu"; color: "#292329" }
})", QUrl("inline:configuration.qml"));
    std::unique_ptr<QObject> configurationOwned(configurationComponent.create());
    auto *configuration = qobject_cast<QQuickWindow *>(configurationOwned.get());
    QVERIFY2(configuration, qPrintable(configurationComponent.errorString()));
    int configurationRequests = 0;
    int toggleRequests = 0;
    connect(&menu, &Menu::DbusMenu::DbusMenuServer::actionActivated, configuration,
        [&](const QString &id) {
            if (id == QStringLiteral("configuration")) {
                ++configurationRequests;
                configuration->show();
            } else if (id == QStringLiteral("led")) {
                ++toggleRequests;
                tree.items[1].checked = !tree.items[1].checked;
                ++tree.revision;
                if (!menu.publish(tree)) qFatal("fixture checkbox publication failed");
            }
        });
    clickItem(button);
    QTRY_COMPARE(item.recordedIntents.size(), 1);
    QCOMPARE(item.recordedIntents.first().member, QStringLiteral("Activate"));
    clickItem(button, Qt::RightButton);
    QTRY_VERIFY_WITH_TIMEOUT(visibleMenuItem(QStringLiteral("Configuration…")), 5'000);
    auto *configurationAction = visibleMenuItem(QStringLiteral("Configuration…"));
    QVERIFY(insideScreen(configurationAction->window()));
    QVERIFY(capture(&panel, directory + "/panel.png"));
    QVERIFY(capture(configurationAction->window(), directory + "/root-menu.png"));
    auto *checkbox = visibleMenuItem(QStringLiteral("Battery-color mouse LED"));
    QVERIFY(checkbox);
    clickItem(checkbox);
    QTRY_COMPARE(toggleRequests, 1);
    clickItem(button, Qt::RightButton);
    QTRY_VERIFY(visibleMenuItem(QStringLiteral("Battery-color mouse LED")));
    QTRY_VERIFY(visibleMenuItem(QStringLiteral("Battery-color mouse LED"))->property("checked").toBool());
    QVERIFY(capture(visibleMenuItem(QStringLiteral("Battery-color mouse LED"))->window(), directory + "/checkbox-menu.png"));
    auto *submenu = visibleMenuItem(QStringLiteral("Tools"), true);
    QVERIFY(submenu);
    const auto point = submenu->mapToScene(QPointF(submenu->width() / 2, submenu->height() / 2)).toPoint();
    QTest::mouseMove(submenu->window(), point);
    QTRY_VERIFY_WITH_TIMEOUT(visibleMenuItem(QStringLiteral("Command Studio…")), 5'000);
    auto *submenuWindow = visibleMenuItem(QStringLiteral("Command Studio…"))->window();
    QVERIFY(insideScreen(submenuWindow));
    QVERIFY(capture(submenuWindow, directory + "/submenu.png"));
    QTest::keyClick(submenuWindow, Qt::Key_Escape);
    QTRY_VERIFY(visibleMenuItem(QStringLiteral("Configuration…")));
    configurationAction = visibleMenuItem(QStringLiteral("Configuration…"));
    clickItem(configurationAction);
    QTRY_COMPARE(configurationRequests, 1);
    QTRY_VERIFY(configuration->isExposed());
    QVERIFY(capture(configuration, directory + "/configuration.png"));
    QCOMPARE(item.recordedIntents.size(), 1);
    QCOMPARE(toggleRequests, 1);
    QFile evidence(directory + "/evidence.json");
    QVERIFY(evidence.open(QIODevice::WriteOnly));
    const QJsonObject report{{"platform", QGuiApplication::platformName()},
        {"vertical", vertical}, {"configurationRequests", configurationRequests},
        {"toggleRequests", toggleRequests}, {"legacyActivateRequests", static_cast<int>(item.recordedIntents.size())},
        {"configurationExposed", configuration->isExposed()}, {"screenScale", panel.devicePixelRatio()}};
    QVERIFY(evidence.write(QJsonDocument(report).toJson()) > 0);
}

QTEST_MAIN(TrayNativeProbe)
#include "tray_native_probe.moc"
