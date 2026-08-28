// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/app_shell/application_coordinator.h"
#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

using namespace QindaQt::AppShell;

class ApplicationShellTest final : public QObject {
    Q_OBJECT

private slots:
    void rendersAccessibleFocusAndDegradedState();
};

void ApplicationShellTest::rendersAccessibleFocusAndDegradedState()
{
    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_QML_IMPORT_PATH));

    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml",
                         QUrl(QStringLiteral("inline:app-shell-token-registration.qml")));
    QTRY_VERIFY_WITH_TIMEOUT(registration.status() != QQmlComponent::Loading, 5000);
    QVERIFY2(registration.isReady(), qPrintable(registration.errorString()));
    std::unique_ptr<QObject> registrationObject(registration.create());
    QVERIFY2(registrationObject != nullptr, qPrintable(registration.errorString()));

    auto *tokens = engine.singletonInstance<QindaQt::DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    QVERIFY(tokens != nullptr);
    const auto theme = QindaQt::Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QVERIFY2(theme.ok, qPrintable(theme.error));
    QString publishError;
    QVERIFY2(tokens->publish(theme.theme, {}, &publishError), qPrintable(publishError));

    QQmlComponent component(&engine,
                            QUrl::fromLocalFile(QStringLiteral(
                                QINDAQT_APP_SHELL_TEST_QML_DIR "/AppShellTestScene.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> root(component.create());
    QVERIFY2(root != nullptr, qPrintable(component.errorString()));
    auto *window = qobject_cast<QQuickWindow *>(root.get());
    QVERIFY(window != nullptr);
    QTRY_VERIFY(window->isExposed());

    auto *coordinator = root->findChild<ApplicationCoordinator *>(
        QStringLiteral("applicationShellCoordinator"));
    auto *primary = root->findChild<QQuickItem *>(QStringLiteral("primaryAction"));
    auto *page = root->findChild<QQuickItem *>(QStringLiteral("appShellPageHost"));
    auto *notice = root->findChild<QQuickItem *>(QStringLiteral("appShellDegradedNotice"));
    QVERIFY(coordinator != nullptr);
    QVERIFY(primary != nullptr);
    QVERIFY(page != nullptr);
    QVERIFY(notice != nullptr);
    QTRY_VERIFY(primary->hasActiveFocus());
    QCOMPARE(coordinator->focusOwnerObjectName(), QStringLiteral("primaryAction"));

    auto *accessible = QAccessible::queryAccessibleInterface(window);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->text(QAccessible::Name),
             QStringLiteral("AppShell test window"));
    auto *pageAccessible = QAccessible::queryAccessibleInterface(page);
    QVERIFY(pageAccessible != nullptr);
    QCOMPARE(pageAccessible->role(), QAccessible::Pane);
    QCOMPARE(pageAccessible->text(QAccessible::Name),
             QStringLiteral("AppShell test application"));

    QVERIFY(!notice->isVisible());
    coordinator->setSettingsState(IntegrationState::Unavailable,
                                  QStringLiteral("Using local defaults."));
    QTRY_VERIFY(notice->isVisible());
    QVERIFY(coordinator->degradedMessage().contains(QStringLiteral("local defaults")));
    QTRY_VERIFY(pageAccessible->text(QAccessible::Description)
                    .contains(QStringLiteral("local defaults")));
}

QTEST_MAIN(ApplicationShellTest)
#include "tst_application_shell.moc"
