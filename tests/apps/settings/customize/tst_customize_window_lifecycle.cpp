// SPDX-License-Identifier: GPL-3.0-or-later
#include "stub_customize_settings_model.h"

#include "src/apps/settings_center/settings_navigation_controller.h"
#include "src/apps/settings_center/settings_route_registry.h"

#include "qindaqt/apps/settings_appearance/appearance_qml_composition.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlPropertyMap>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

using QindaQt::Apps::SettingsCenter::SettingsNavigationController;
using QindaQt::Apps::SettingsCenter::SettingsRouteRegistry;
using QindaQt::Apps::SettingsCustomize::TestSupport::StubCustomizeSettingsModel;

namespace {

QObject *visibleDialog(QObject *root)
{
    const auto dialogs = root->findChildren<QObject *>(
        QStringLiteral("customizeDiscardDialog"));
    for (QObject *dialog : dialogs) {
        if (dialog->property("visible").toBool()) {
            return dialog;
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
    QObject appearance;
    std::unique_ptr<QObject> root;
    QQuickWindow *window = nullptr;
    QString failure;
};

} // namespace

class CustomizeWindowLifecycleTests final : public QObject {
    Q_OBJECT

private slots:
    void dirtyWindowCloseRequiresDecision();
    void pendingDepartureSurvivesResponsiveHostSwitch();
    void pendingApplicationCloseSurvivesResponsiveHostSwitch();
};

void CustomizeWindowLifecycleTests::dirtyWindowCloseRequiresDecision()
{
    WindowHarness harness;
    QVERIFY2(harness.window != nullptr, qPrintable(harness.failure));
    harness.window->resize(720, 520);
    harness.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(harness.window));
    harness.customize.setDirty(true);

    QVERIFY(!harness.window->close());
    QTRY_VERIFY(harness.window->isVisible());
    QTRY_VERIFY(visibleDialog(harness.root.get()) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(visibleDialog(harness.root.get()), "reject"));
    QTRY_VERIFY(harness.window->isVisible());
    QVERIFY(harness.customize.dirty());
}

void CustomizeWindowLifecycleTests::pendingDepartureSurvivesResponsiveHostSwitch()
{
    WindowHarness harness;
    QVERIFY2(harness.window != nullptr, qPrintable(harness.failure));
    harness.window->resize(720, 520);
    harness.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(harness.window));
    harness.customize.setDirty(true);

    QVERIFY(harness.navigation.selectRoute(QStringLiteral("notifications")));
    QTRY_VERIFY(visibleDialog(harness.root.get()) != nullptr);
    harness.window->resize(440, 360);
    QTRY_VERIFY(harness.root->property("isCompact").toBool());
    QTRY_VERIFY(visibleDialog(harness.root.get()) != nullptr);
    QVERIFY(QMetaObject::invokeMethod(visibleDialog(harness.root.get()), "reject"));
    QTRY_COMPARE(harness.navigation.activeRouteId(), QStringLiteral("customize"));
    QVERIFY(harness.customize.dirty());
}

void CustomizeWindowLifecycleTests::pendingApplicationCloseSurvivesResponsiveHostSwitch()
{
    const auto verifyDirection = [](const QSize &initialSize,
                                    const QSize &resizedSize,
                                    const char *targetHostName) {
        WindowHarness harness;
        QVERIFY2(harness.window != nullptr, qPrintable(harness.failure));
        harness.window->resize(initialSize);
        harness.window->show();
        QVERIFY(QTest::qWaitForWindowExposed(harness.window));
        harness.customize.setDirty(true);

        QVERIFY(!harness.window->close());
        QTRY_VERIFY(visibleDialog(harness.root.get()) != nullptr);
        QVERIFY(harness.root->property("applicationClosePending").toBool());
        harness.window->resize(resizedSize);
        QTRY_COMPARE(harness.window->size(), resizedSize);
        auto *targetHost = harness.root->findChild<QObject *>(
            QString::fromLatin1(targetHostName));
        QVERIFY(targetHost != nullptr);
        QTRY_VERIFY(visibleDialog(targetHost) != nullptr);
        QVERIFY(harness.root->property("applicationClosePending").toBool());
        QVERIFY(harness.window->isVisible());
        QVERIFY(harness.customize.dirty());

        QVERIFY(QMetaObject::invokeMethod(visibleDialog(targetHost), "reject"));
        QTRY_VERIFY(!harness.root->property("applicationClosePending").toBool());
        QTRY_VERIFY(harness.window->isVisible());
        QVERIFY(harness.customize.dirty());
    };

    verifyDirection(QSize{720, 520}, QSize{440, 360},
                    "compactSettingsRouteHost");
    verifyDirection(QSize{440, 360}, QSize{720, 520},
                    "wideSettingsRouteHost");
}

QTEST_MAIN(CustomizeWindowLifecycleTests)
#include "tst_customize_window_lifecycle.moc"
