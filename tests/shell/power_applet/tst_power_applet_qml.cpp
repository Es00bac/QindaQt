// SPDX-License-Identifier: GPL-3.0-or-later

#include "power_applet_controller.h"

#include "support/fake_power_transport.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_PowerAppletPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::PowerApplet;
using namespace QindaQt::Tests;

namespace {

const QString kOwner = QStringLiteral(":1.42");

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
    PowerAppletController controller(&client, true, true);
    publishReady(client, transport);

    QQmlEngine engine;
    engine.addImportPath(
        QStringLiteral(QINDAQT_POWER_APPLET_QML_IMPORT_PATH));
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
}

QTEST_MAIN(PowerAppletQmlTests)
#include "tst_power_applet_qml.moc"
