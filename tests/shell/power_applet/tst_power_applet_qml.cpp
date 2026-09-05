// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_applet_controller.h"
#include "../icon_resolution_test_fixture.h"

#include "support/fake_power_transport.h"

#include "qindaqt/design_tokens/token_facade.h"
#include "qindaqt/themes/theme_loader.h"

#include <QAccessible>
#include <QEventLoop>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_PowerAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::PowerApplet;
using namespace QindaQt::Tests;

namespace {

const QString kOwner = QStringLiteral(":1.42");

class StubSessionActions final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool canLock MEMBER canLock NOTIFY availabilityChanged)
    Q_PROPERTY(bool canLogout MEMBER canLogout NOTIFY availabilityChanged)
    Q_PROPERTY(bool canSuspend MEMBER canSuspend NOTIFY availabilityChanged)
    Q_PROPERTY(bool canReboot MEMBER canReboot NOTIFY availabilityChanged)
    Q_PROPERTY(bool canPowerOff MEMBER canPowerOff NOTIFY availabilityChanged)
    Q_PROPERTY(bool pending MEMBER pending NOTIFY pendingChanged)
    Q_PROPERTY(QString feedback MEMBER feedback NOTIFY feedbackChanged)

public:
    bool canLock = true;
    bool canLogout = true;
    bool canSuspend = true;
    bool canReboot = true;
    bool canPowerOff = true;
    bool pending = false;
    QString feedback;
    QStringList requests;

    Q_INVOKABLE bool requestLock() { requests.append(QStringLiteral("lock")); return true; }
    Q_INVOKABLE bool requestLogout() { requests.append(QStringLiteral("logout")); return true; }
    Q_INVOKABLE bool requestSuspend() { requests.append(QStringLiteral("suspend")); return true; }
    Q_INVOKABLE bool requestReboot() { requests.append(QStringLiteral("reboot")); return true; }
    Q_INVOKABLE bool requestPowerOff() { requests.append(QStringLiteral("poweroff")); return true; }

Q_SIGNALS:
    void availabilityChanged();
    void pendingChanged();
    void feedbackChanged();
};

QVariantMap testTheme()
{
    return {{QStringLiteral("cornerRadius"), 8},
            {QStringLiteral("colors"),
             QVariantMap{{QStringLiteral("surfaceRaised"),
                          QStringLiteral("#2c312e")},
                         {QStringLiteral("border"), QStringLiteral("#3c433f")},
                         {QStringLiteral("text"), QStringLiteral("#f2f1eb")},
                         {QStringLiteral("textMuted"), QStringLiteral("#a9afa9")},
                         {QStringLiteral("warning"), QStringLiteral("#e5a84b")}}}};
}

bool publishTokens(QQmlEngine &engine)
{
    QQmlComponent registration(&engine);
    registration.setData(R"qml(
        import QtQuick
        import QindaQt.Tokens 1.0
        QtObject { property int revision: Tokens.qstRevision }
    )qml", QUrl(QStringLiteral("inline:power-token-registration.qml")));
    if (registration.status() == QQmlComponent::Loading) {
        QEventLoop loop;
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        QObject::connect(&registration, &QQmlComponent::statusChanged, &loop,
                         [&loop](QQmlComponent::Status status) {
            if (status != QQmlComponent::Loading)
                loop.quit();
        });
        loop.exec();
    }
    if (!registration.isReady())
        return false;
    std::unique_ptr<QObject> registrationObject(registration.create());
    auto *facade = engine.singletonInstance<DesignTokens::TokenFacade *>(
        "QindaQt.Tokens", "Tokens");
    const auto loaded = Themes::ThemeLoader::fromFile(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes/qinda-dark.json"));
    QString error;
    return registrationObject != nullptr && facade != nullptr && loaded.ok
        && facade->publish(loaded.theme, {}, &error);
}

void publishReady(Power::PowerClient &client, FakePowerTransport &transport)
{
    client.start();
    transport.announceOwner(kOwner);
    transport.reply(transport.fetches.constLast(), powerClientSnapshot());
    QCOMPARE(client.state(), Power::PowerClientState::Ready);
}

QList<QQuickItem *> visualItemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> matches;
    if (root->objectName() == name) {
        matches.append(root);
    }
    for (QQuickItem *child : root->childItems()) {
        matches.append(visualItemsNamed(child, name));
    }
    return matches;
}

} // namespace

class PowerAppletQmlTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compiledAppletSupportsKeyboardAndAccessibility();
};

void PowerAppletQmlTests::compiledAppletSupportsKeyboardAndAccessibility()
{
    FakePowerTransport transport;
    Power::PowerClient client(&transport);
    StubSessionActions sessionActions;
    PowerAppletController controller(&client, true, true, &sessionActions);
    publishReady(client, transport);

    QQmlEngine engine;
    engine.addImportPath(
        QStringLiteral(QINDAQT_POWER_APPLET_QML_IMPORT_PATH));
    QString iconError;
    QVERIFY2(installResolvedIconFixture(
                 engine, QStringLiteral(QINDAQT_APPLET_ICON_FIXTURE_ROOT),
                 {QStringLiteral("battery-060")}, &iconError),
             qPrintable(iconError));
    QVERIFY(publishTokens(engine));
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.PowerApplet"),
                             QStringLiteral("PowerApplet"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> owned(component.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue(&controller)},
         {QStringLiteral("theme"), testTheme()}}));
    QVERIFY2(owned != nullptr, qPrintable(component.errorString()));
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);

    QQuickWindow window;
    window.setGeometry(0, 0, 420, 520);
    root->setParentItem(window.contentItem());
    root->setPosition(QPointF(20, 20));
    window.show();
    QTRY_VERIFY(window.isExposed());

    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("powerAppletSummary"));
    QVERIFY(summary != nullptr);
    QCOMPARE(root->width(), 62.0);
    QCOMPARE(root->height(), 28.0);
    QCOMPARE(summary->width(), root->width());
    auto *summaryIcon = summary->findChild<QQuickItem *>(
        QStringLiteral("powerAppletIcon"));
    QVERIFY(summaryIcon != nullptr);
    QVERIFY(hasResolvedProviderSource(summaryIcon,
                                      QStringLiteral("battery-060")));
    summary->forceActiveFocus();
    QVERIFY(summary->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Space);

    QObject *popup = root->findChild<QObject *>(
        QStringLiteral("powerAppletPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());

    QAccessibleInterface *summaryInterface =
        QAccessible::queryAccessibleInterface(summary);
    QVERIFY(summaryInterface != nullptr);
    QCOMPARE(summaryInterface->role(), QAccessible::Button);
    QVERIFY(summaryInterface->text(QAccessible::Name).contains(
        QStringLiteral("56%")));

    const auto profileButtons = visualItemsNamed(
        window.contentItem(), QStringLiteral("powerAppletProfileButton"));
    QCOMPARE(profileButtons.size(), 2);
    QQuickItem *powerSaver = nullptr;
    for (QQuickItem *button : profileButtons) {
        if (button->property("text").toString().startsWith(
                QStringLiteral("Power Saver"))) {
            powerSaver = button;
            break;
        }
    }
    QVERIFY(powerSaver != nullptr);
    QAccessibleInterface *profileInterface =
        QAccessible::queryAccessibleInterface(powerSaver);
    QVERIFY(profileInterface != nullptr);
    QCOMPARE(profileInterface->role(), QAccessible::RadioButton);
    QVERIFY(!profileInterface->text(QAccessible::Description).isEmpty());

    const auto sliders = visualItemsNamed(
        window.contentItem(), QStringLiteral("powerAppletKeyboardSlider"));
    QCOMPARE(sliders.size(), 1);
    QQuickItem *slider = sliders.constFirst();
    QAccessibleInterface *sliderInterface =
        QAccessible::queryAccessibleInterface(slider);
    QVERIFY(sliderInterface != nullptr);
    QCOMPARE(sliderInterface->role(), QAccessible::Slider);
    QVERIFY(!sliderInterface->text(QAccessible::Name).isEmpty());
    QVERIFY(!sliderInterface->text(QAccessible::Description).isEmpty());

    slider->forceActiveFocus();
    QVERIFY(slider->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Right);
    QTRY_COMPARE(transport.operations.size(), 1);
    QCOMPARE(transport.operations.constFirst().request.kind,
             Power::OperationKind::SetKeyboardBrightness);
    QVERIFY(controller.operationPending());

    transport.finish(
        transport.operations.constFirst(),
        powerClientResult(transport.operations.constFirst(),
                          Power::OperationStatus::Succeeded,
                          QStringLiteral("applied")));
    QTRY_VERIFY(!controller.operationPending());

    powerSaver->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    QTRY_COMPARE(transport.operations.size(), 2);
    QCOMPARE(transport.operations.constLast().request.kind,
             Power::OperationKind::SetProfile);
    QCOMPARE(transport.operations.constLast().request.profileId,
             QStringLiteral("power-saver"));

    auto *lock = root->findChild<QQuickItem *>(
        QStringLiteral("powerAppletLockButton"));
    auto *suspend = root->findChild<QQuickItem *>(
        QStringLiteral("powerAppletSuspendButton"));
    auto *restart = root->findChild<QQuickItem *>(
        QStringLiteral("powerAppletRestartButton"));
    QVERIFY(lock != nullptr);
    QVERIFY(suspend != nullptr);
    QVERIFY(restart != nullptr);
    QAccessibleInterface *lockInterface =
        QAccessible::queryAccessibleInterface(lock);
    QVERIFY(lockInterface != nullptr);
    QCOMPARE(lockInterface->role(), QAccessible::Button);
    QVERIFY(lockInterface->text(QAccessible::Description)
                .contains(QStringLiteral("Lock")));

    lock->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    QCOMPARE(sessionActions.requests,
             QStringList({QStringLiteral("lock")}));
    suspend->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    QCOMPARE(sessionActions.requests.constLast(), QStringLiteral("suspend"));
    restart->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    QCOMPARE(sessionActions.requests.size(), 2);
    QObject *confirmation = root->findChild<QObject *>(
        QStringLiteral("powerAppletSessionConfirmation"));
    QVERIFY(confirmation != nullptr);
    QTRY_VERIFY(confirmation->property("opened").toBool());
    QVERIFY(QMetaObject::invokeMethod(confirmation, "accept"));
    QTRY_COMPARE(sessionActions.requests.size(), 3);
    QCOMPARE(sessionActions.requests.constLast(), QStringLiteral("reboot"));
}

QTEST_MAIN(PowerAppletQmlTests)
#include "tst_power_applet_qml.moc"
