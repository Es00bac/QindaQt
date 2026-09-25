// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_customize_settings_model.h"

#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/shell/icons/icon_runtime.h"
#include "qindaqt/themes/theme_loader.h"

#include <QQmlComponent>
#include <QQmlExtensionPlugin>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using QindaQt::Apps::SettingsCenter::SettingsNavigationController;
using QindaQt::Apps::SettingsCenter::SettingsRouteRegistry;
using QindaQt::Apps::SettingsCustomize::TestSupport::StubCustomizeSettingsModel;

namespace {

QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root == nullptr) {
        return nullptr;
    }
    if (root->objectName() == name) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (auto *match = findItem(child, name); match != nullptr) {
            return match;
        }
    }
    return nullptr;
}

class WindowHarness final {
public:
    WindowHarness()
        : navigation(SettingsRouteRegistry::createDefault(),
                     QStringLiteral("customize"))
    {
        quieting->insert(QStringLiteral("enabled"), false);
        quieting->insert(QStringLiteral("canToggle"), false);
        quieting->insert(QStringLiteral("conflict"), false);
        quieting->insert(QStringLiteral("unavailable"), false);
        quieting->insert(QStringLiteral("statusText"), QString{});
        quieting->insert(QStringLiteral("errorText"), QString{});
        // AGENT-NOTE: O13 made `quietingSchedule` a required property of
        // Main.qml, so the component cannot be created without one, and it is
        // a SEPARATE model from `quietingSettings` — QuietHoursSection reads
        // availability, editability, schedule times, status, conflict, and
        // uncertainty from it. Handing it the settings stand-in leaves those
        // undefined, and this harness runs with fatal QML warnings. Every
        // construction-bound key needs its real value type; errorText is also
        // read as `errorText.length`. The mutation methods are event-only and
        // are not called by this Customize lifecycle test.
        schedule->insert(QStringLiteral("available"), false);
        schedule->insert(QStringLiteral("canEdit"), true);
        schedule->insert(QStringLiteral("scheduleEnabled"), false);
        schedule->insert(QStringLiteral("startMinutes"), 22 * 60);
        schedule->insert(QStringLiteral("endMinutes"), 7 * 60);
        schedule->insert(QStringLiteral("startText"), QStringLiteral("22:00"));
        schedule->insert(QStringLiteral("endText"), QStringLiteral("07:00"));
        schedule->insert(QStringLiteral("summaryText"), QString{});
        schedule->insert(QStringLiteral("statusText"), QString{});
        schedule->insert(QStringLiteral("conflict"), false);
        schedule->insert(QStringLiteral("uncertain"), false);
        schedule->insert(QStringLiteral("errorText"), QString{});
        engine.addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));
        QString error;
        auto *facade = QindaQt::Apps::SettingsAppearance::ensureTokenFacade(
            engine, &error);
        if (facade == nullptr) {
            failure = error;
            return;
        }
        const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
            QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
        if (!theme.ok || !facade->publish(theme.theme, {}, &error)) {
            failure = theme.ok ? error : theme.error;
            return;
        }
        if (!QindaQt::Shell::Icons::IconRuntime::install(
                engine, {QStringLiteral(QINDAQT_SOURCE_DIR "/data/icons")},
                {QStringLiteral("QindaQt")})) {
            failure = QStringLiteral("icon runtime install");
            return;
        }

        QQmlComponent component(
            &engine,
            QUrl::fromLocalFile(QStringLiteral(QINDAQT_SETTINGS_SOURCE_DIR
                                               "/Main.qml")));
        if (!component.isReady()) {
            failure = component.errorString();
            return;
        }
        root.reset(component.createWithInitialProperties({
            {QStringLiteral("navigation"),
             QVariant::fromValue(static_cast<QObject *>(&navigation))},
            {QStringLiteral("quietingSettings"),
             QVariant::fromValue(static_cast<QObject *>(quieting.get()))},
            {QStringLiteral("quietingSchedule"),
             QVariant::fromValue(static_cast<QObject *>(schedule.get()))},
            {QStringLiteral("appearanceSettings"),
             QVariant::fromValue(static_cast<QObject *>(&appearance))},
            {QStringLiteral("customizeSettings"),
             QVariant::fromValue(static_cast<QObject *>(&customize))},
        }));
        window = qobject_cast<QQuickWindow *>(root.get());
    }

    QQmlEngine engine;
    SettingsNavigationController navigation;
    StubCustomizeSettingsModel customize;
    std::unique_ptr<QQmlPropertyMap> quieting{QQmlPropertyMap::create()};
    std::unique_ptr<QQmlPropertyMap> schedule{QQmlPropertyMap::create()};
    QObject appearance;
    std::unique_ptr<QObject> root;
    QQuickWindow *window = nullptr;
    QString failure;
};

} // namespace

// ADR-0267: the preset page holds no draft, so the Settings Center's
// Customize departure fence (SettingsRouteHost, Main.qml) never engages: the
// model's dirty flag is always false. These rows construct the page inside
// the real Main.qml in both responsive hosts and prove that closing the
// window or leaving the route never prompts or bounces back.
class CustomizeWindowLifecycleTests final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void presetPageConstructsInBothHosts();
    void closingTheWindowNeverPrompts();
    void leavingTheRouteNeverBouncesBack();
};

void CustomizeWindowLifecycleTests::initTestCase()
{
    // One row closes its window for real; the harness keeps running.
    QGuiApplication::setQuitOnLastWindowClosed(false);
}

void CustomizeWindowLifecycleTests::presetPageConstructsInBothHosts()
{
    WindowHarness harness;
    QVERIFY2(harness.window != nullptr, qPrintable(harness.failure));
    harness.window->resize(720, 520);
    harness.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(harness.window));
    auto *wide = harness.root->findChild<QQuickItem *>(QStringLiteral("wideSettingsRouteHost"));
    QVERIFY(wide != nullptr);
    QTRY_VERIFY(findItem(wide, QStringLiteral("customizeProfileCard_fixture")) != nullptr);

    harness.window->resize(440, 360);
    QTRY_VERIFY(harness.root->property("isCompact").toBool());
    auto *compact = harness.root->findChild<QQuickItem *>(
        QStringLiteral("compactSettingsRouteHost"));
    QVERIFY(compact != nullptr);
    QTRY_VERIFY(findItem(compact, QStringLiteral("customizeProfileCard_fixture")) != nullptr);
}

void CustomizeWindowLifecycleTests::closingTheWindowNeverPrompts()
{
    WindowHarness harness;
    QVERIFY2(harness.window != nullptr, qPrintable(harness.failure));
    harness.window->resize(720, 520);
    harness.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(harness.window));
    QVERIFY(!harness.customize.property("dirty").toBool());

    QVERIFY(harness.window->close());
    QVERIFY(!harness.root->property("applicationClosePending").toBool());
    QTRY_VERIFY(!harness.window->isVisible());
}

void CustomizeWindowLifecycleTests::leavingTheRouteNeverBouncesBack()
{
    WindowHarness harness;
    QVERIFY2(harness.window != nullptr, qPrintable(harness.failure));
    harness.window->resize(720, 520);
    harness.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(harness.window));

    QVERIFY(harness.navigation.selectRoute(QStringLiteral("notifications")));
    QTest::qWait(50);
    QCOMPARE(harness.navigation.activeRouteId(), QStringLiteral("notifications"));
    auto *customizeLoader = harness.root->findChild<QObject *>(
        QStringLiteral("wideSettingsRouteCustomizeLoader"));
    QVERIFY(customizeLoader != nullptr);
    QTRY_VERIFY(!customizeLoader->property("active").toBool());
    QVERIFY(harness.window->isVisible());
}

QTEST_MAIN(CustomizeWindowLifecycleTests)
#include "tst_customize_window_lifecycle.moc"
