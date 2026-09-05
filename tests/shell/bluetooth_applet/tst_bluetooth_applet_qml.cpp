// SPDX-License-Identifier: GPL-3.0-or-later

#include "bluetooth_applet_controller.h"
#include "../icon_resolution_test_fixture.h"

#include "support/fake_bluetooth_transport.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_BluetoothAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::BluetoothApplet;
using namespace QindaQt::Tests;

namespace
{

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

void publishReady(Bluetooth::BluetoothClient &client,
                  FakeBluetoothTransport &transport)
{
    client.start();
    transport.setOwner(kOwner);
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, bluetoothClientSnapshot());
    QCOMPARE(client.state(), Bluetooth::ClientState::Ready);
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

Bluetooth::OperationResult resultFor(
    const FakeBluetoothTransport::RecordedSubmission &submission,
    const Bluetooth::OperationStatus status,
    const QString &reason,
    const quint64 observedRevision,
    const quint64 initiatingRevision = 5)
{
    return {.kind = submission.request.kind,
            .status = status,
            .initiatingEpoch = 61,
            .initiatingRevision = initiatingRevision,
            .observedEpoch = 61,
            .observedRevision = observedRevision,
            .reasonCode = reason,
            .diagnostic = {},
            .wireValid = true};
}

} // namespace

class BluetoothAppletQmlTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void compiledAppletSupportsKeyboardAccessibilityAndLeaseClose();
    void pairingPromptHasKeyboardAccessibleActions();
};

void BluetoothAppletQmlTests::compiledAppletSupportsKeyboardAccessibilityAndLeaseClose()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    publishReady(client, transport);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_BLUETOOTH_APPLET_QML_IMPORT_PATH));
    QString iconError;
    QVERIFY2(installResolvedIconFixture(
                 engine, QStringLiteral(QINDAQT_APPLET_ICON_FIXTURE_ROOT),
                 {QStringLiteral("network-bluetooth-activated")}, &iconError),
             qPrintable(iconError));
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.BluetoothApplet"),
                             QStringLiteral("BluetoothApplet"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> owned(component.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue(&controller)},
         {QStringLiteral("theme"), testTheme()}}));
    QVERIFY2(owned != nullptr, qPrintable(component.errorString()));
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);

    QQuickWindow window;
    window.setGeometry(0, 0, 480, 600);
    root->setParentItem(window.contentItem());
    root->setPosition(QPointF(20, 20));
    window.show();
    QTRY_VERIFY(window.isExposed());

    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("bluetoothAppletSummary"));
    QVERIFY(summary != nullptr);
    QCOMPARE(root->width(), 32.0);
    QCOMPARE(root->height(), 28.0);
    QCOMPARE(summary->property("text").toString(), QString());
    auto *summaryIcon = summary->findChild<QQuickItem *>(
        QStringLiteral("bluetoothAppletIcon"));
    QVERIFY(summaryIcon != nullptr);
    QVERIFY(hasResolvedProviderSource(
        summaryIcon, QStringLiteral("network-bluetooth-activated")));
    summary->forceActiveFocus();
    QVERIFY(summary->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Space);

    QObject *popup = root->findChild<QObject *>(
        QStringLiteral("bluetoothAppletPopup"));
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("opened").toBool());

    QAccessibleInterface *summaryInterface =
        QAccessible::queryAccessibleInterface(summary);
    QVERIFY(summaryInterface != nullptr);
    QCOMPARE(summaryInterface->role(), QAccessible::Button);
    QVERIFY(summaryInterface->text(QAccessible::Name).contains(
        QStringLiteral("Bluetooth")));
    QVERIFY(!summaryInterface->text(QAccessible::Description).isEmpty());

    const auto discoveryButtons = visualItemsNamed(
        window.contentItem(), QStringLiteral("bluetoothAppletDiscoveryButton"));
    QCOMPARE(discoveryButtons.size(), 1);
    QQuickItem *discovery = discoveryButtons.constFirst();
    QAccessibleInterface *discoveryInterface =
        QAccessible::queryAccessibleInterface(discovery);
    QVERIFY(discoveryInterface != nullptr);
    QCOMPARE(discoveryInterface->role(), QAccessible::Button);
    QVERIFY(!discoveryInterface->text(QAccessible::Name).isEmpty());
    QVERIFY(!discoveryInterface->text(QAccessible::Description).isEmpty());

    discovery->forceActiveFocus();
    QVERIFY(discovery->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Space);
    QTRY_COMPARE(transport.submissions.size(), 1);
    const auto acquire = transport.submissions.constFirst();
    QCOMPARE(acquire.request.kind, Bluetooth::OperationKind::AcquireDiscovery);

    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("opened").toBool());
    transport.emitOperationReply(
        kOwner, acquire.requestId, true,
        resultFor(acquire, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-acquired"), 6));
    QTRY_COMPARE(transport.fetches.size(), 2);
    Bluetooth::Snapshot acquired = bluetoothClientSnapshot(61, 6);
    acquired.adapters[0].discovering = true;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, acquired);
    QTRY_COMPARE(transport.submissions.size(), 2);
    const auto release = transport.submissions.constLast();
    QCOMPARE(release.request.kind, Bluetooth::OperationKind::ReleaseDiscovery);

    transport.emitOperationReply(
        kOwner, release.requestId, true,
        resultFor(release, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("lease-released"), 7, 6));
    QTRY_VERIFY(!controller.discoveryLeaseHeld());
    QTRY_COMPARE(transport.fetches.size(), 3);
    Bluetooth::Snapshot released = bluetoothClientSnapshot(61, 7);
    released.adapters[0].discovering = false;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, released);
    QTRY_VERIFY(!controller.operationPending());
}

void BluetoothAppletQmlTests::pairingPromptHasKeyboardAccessibleActions()
{
    FakeBluetoothTransport transport;
    Bluetooth::BluetoothClient client(&transport);
    BluetoothAppletController controller(&client, true, true);
    Bluetooth::Snapshot prompt = bluetoothClientSnapshot();
    prompt.pairingPrompt = {
        .promptId = 104,
        .kind = Bluetooth::PairingPromptKind::ConfirmPasskey,
        .device = prompt.devices.constFirst().handle,
        .detail = QStringLiteral("123456"),
        .serviceUuid = {},
        .entered = 0,
    };
    client.start();
    transport.setOwner(kOwner);
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, prompt);

    QQmlEngine engine;
    engine.addImportPath(QStringLiteral(QINDAQT_BLUETOOTH_APPLET_QML_IMPORT_PATH));
    QQmlComponent component(&engine);
    component.loadFromModule(QStringLiteral("QindaQt.Shell.BluetoothApplet"),
                             QStringLiteral("BluetoothApplet"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> owned(component.createWithInitialProperties(
        {{QStringLiteral("access"), QVariant::fromValue(&controller)},
         {QStringLiteral("theme"), testTheme()}}));
    QVERIFY2(owned != nullptr, qPrintable(component.errorString()));
    auto *root = qobject_cast<QQuickItem *>(owned.get());
    QVERIFY(root != nullptr);
    QQuickWindow window;
    window.setGeometry(0, 0, 480, 600);
    root->setParentItem(window.contentItem());
    window.show();
    QTRY_VERIFY(window.isExposed());
    auto *summary = root->findChild<QQuickItem *>(
        QStringLiteral("bluetoothAppletSummary"));
    summary->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    QTRY_VERIFY(root->findChild<QObject *>(
        QStringLiteral("bluetoothAppletPopup"))->property("opened").toBool());

    auto *confirm = root->findChild<QQuickItem *>(
        QStringLiteral("bluetoothAppletPairingConfirm"));
    auto *cancel = root->findChild<QQuickItem *>(
        QStringLiteral("bluetoothAppletPairingCancel"));
    QVERIFY(confirm != nullptr);
    QVERIFY(cancel != nullptr);
    QAccessibleInterface *confirmInterface =
        QAccessible::queryAccessibleInterface(confirm);
    QAccessibleInterface *cancelInterface =
        QAccessible::queryAccessibleInterface(cancel);
    QVERIFY(confirmInterface != nullptr);
    QVERIFY(cancelInterface != nullptr);
    QCOMPARE(confirmInterface->role(), QAccessible::Button);
    QCOMPARE(cancelInterface->role(), QAccessible::Button);
    QVERIFY(!confirmInterface->text(QAccessible::Description).isEmpty());
    QVERIFY(!cancelInterface->text(QAccessible::Description).isEmpty());
    confirm->forceActiveFocus();
    QTest::keyClick(&window, Qt::Key_Space);
    QTRY_COMPARE(transport.submissions.size(), 1);
    QCOMPARE(transport.submissions.constFirst().request.kind,
             Bluetooth::OperationKind::ReplyConfirmation);
    QVERIFY(transport.submissions.constFirst().request.accepted);

    const auto confirmation = transport.submissions.constFirst();
    transport.emitOperationReply(
        kOwner, confirmation.requestId, true,
        resultFor(confirmation, Bluetooth::OperationStatus::Succeeded,
                  QStringLiteral("prompt-replied"), 6));
    QTRY_COMPARE(transport.fetches.size(), 2);
    Bluetooth::Snapshot replacement = bluetoothClientSnapshot(61, 6);
    replacement.pairingPrompt = prompt.pairingPrompt;
    replacement.pairingPrompt.promptId = 105;
    transport.emitSnapshotReply(kOwner, transport.fetches.constLast().requestId,
                                true, replacement);
    QTRY_VERIFY(!controller.pairingReplyPending());

    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_COMPARE(transport.submissions.size(), 2);
    QCOMPARE(transport.submissions.constLast().request.kind,
             Bluetooth::OperationKind::ReplyConfirmation);
    QVERIFY(!transport.submissions.constLast().request.accepted);
    QTRY_VERIFY(!root->findChild<QObject *>(
        QStringLiteral("bluetoothAppletPopup"))->property("opened").toBool());
}

QTEST_MAIN(BluetoothAppletQmlTests)
#include "tst_bluetooth_applet_qml.moc"
