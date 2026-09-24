// SPDX-License-Identifier: GPL-3.0-or-later

#include "network_applet_controller.h"
#include "network_applet_test_support.h"
#include "../icon_resolution_test_fixture.h"
#include "desktop_controls_qml_test_support.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_NetworkAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt;
using namespace QindaQt::Shell::NetworkApplet;
using namespace QindaQt::Shell::NetworkApplet::TestSupport;
using Network::OperationKind;
using Network::OperationStatus;

namespace
{

QVariantMap testTheme()
{
    return {{QStringLiteral("cornerRadius"), 8},
            {QStringLiteral("colors"),
             QVariantMap{{QStringLiteral("surfaceRaised"), QStringLiteral("#2c312e")},
                         {QStringLiteral("border"), QStringLiteral("#3c433f")},
                         {QStringLiteral("text"), QStringLiteral("#f2f1eb")},
                         {QStringLiteral("textMuted"), QStringLiteral("#a9afa9")},
                         {QStringLiteral("warning"), QStringLiteral("#e5a84b")}}}};
}

QList<QQuickItem *> visualItemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> matches;
    if (root->objectName() == name) matches.append(root);
    for (QQuickItem *child : root->childItems()) matches.append(visualItemsNamed(child, name));
    return matches;
}

QQuickItem *popupContent(QQuickItem *root)
{
    auto *popup = root->findChild<QObject *>(QStringLiteral("networkAppletPopup"));
    return popup ? popup->property("contentItem").value<QQuickItem *>() : nullptr;
}

QQuickItem *itemDescribed(QQuickItem *root, const QString &name, const QString &needle)
{
    for (QQuickItem *item : visualItemsNamed(root, name)) {
        QAccessibleInterface *iface = QAccessible::queryAccessibleInterface(item);
        if (item->isVisible() && iface != nullptr
            && iface->text(QAccessible::Description).contains(needle)) {
            return item;
        }
    }
    return nullptr;
}

struct Harness final {
    qint64 now = 3'000;
    FakeNetworkTransport transport;
    Network::Client::NetworkClient client;
    NetworkAppletController controller;
    QQmlEngine engine;
    std::unique_ptr<QObject> owned;
    QQuickItem *root = nullptr;
    QQuickWindow window;

    Harness()
        : client(transport, [this] { return now; }, fastTiming())
        , controller(&client, true, true)
    {
        transport.setSnapshot(appletSnapshot());
        const bool started = client.start();
        Q_ASSERT(started);
        Q_UNUSED(started);
        transport.announceOwner(kOwner);
    }

    bool load(const bool vertical, const QSize panel, QString *error)
    {
        engine.addImportPath(QStringLiteral(QINDAQT_NETWORK_APPLET_QML_IMPORT_PATH));
        // The shell publishes QST-1 before any panel QML exists; so must we,
        // or the shared controls read undefined roles (a fatal warning here).
        if (!Tests::DesktopControls::publishTokens(engine)) {
            *error = QStringLiteral("token publication failed");
            return false;
        }
        if (!Tests::installResolvedIconFixture(
                engine, QStringLiteral(QINDAQT_APPLET_ICON_FIXTURE_ROOT),
                {QStringLiteral("network-wireless-signal-excellent")}, error)) {
            return false;
        }
        QQmlComponent component(&engine);
        component.loadFromModule(QStringLiteral("QindaQt.Shell.NetworkApplet"),
                                 QStringLiteral("NetworkApplet"));
        if (!component.isReady()) {
            *error = component.errorString();
            return false;
        }
        owned.reset(component.createWithInitialProperties(
            {{QStringLiteral("access"), QVariant::fromValue(&controller)},
             {QStringLiteral("theme"), testTheme()},
             {QStringLiteral("vertical"), vertical}}));
        root = qobject_cast<QQuickItem *>(owned.get());
        if (root == nullptr) {
            *error = component.errorString();
            return false;
        }
        window.setGeometry(0, 0, panel.width(), panel.height());
        root->setParentItem(window.contentItem());
        root->setSize(QSizeF(qMin(panel.width(), 32), qMin(panel.height(), 28)));
        window.show();
        return true;
    }

    QObject *popup() const
    {
        return root->findChild<QObject *>(QStringLiteral("networkAppletPopup"));
    }

    void openWithKeyboard()
    {
        auto *summary = root->findChild<QQuickItem *>(QStringLiteral("networkAppletSummary"));
        summary->forceActiveFocus();
        QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    }
};

} // namespace

class NetworkAppletQmlTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void keyboardJoinsANetworkAndSwitchShowsOnlyConfirmedTruth();
    void compactVerticalPanelOpensAccessiblePopup();
};

void NetworkAppletQmlTests::keyboardJoinsANetworkAndSwitchShowsOnlyConfirmedTruth()
{
    Harness h;
    QTRY_COMPARE(h.controller.phase(), QStringLiteral("ready"));
    QString error;
    QVERIFY2(h.load(false, QSize(480, 30), &error), qPrintable(error));
    QTRY_VERIFY(h.window.isExposed());

    auto *summary = h.root->findChild<QQuickItem *>(QStringLiteral("networkAppletSummary"));
    QVERIFY(summary != nullptr);
    QAccessibleInterface *summaryInterface = QAccessible::queryAccessibleInterface(summary);
    QCOMPARE(summaryInterface->role(), QAccessible::Button);
    QVERIFY(summaryInterface->text(QAccessible::Name).contains(QStringLiteral("Home")));
    QVERIFY(!summaryInterface->text(QAccessible::Description).isEmpty());
    auto *icon = summary->findChild<QQuickItem *>(QStringLiteral("networkAppletIcon"));
    QVERIFY(Tests::hasResolvedProviderSource(
        icon, QStringLiteral("network-wireless-signal-excellent")));

    h.openWithKeyboard();
    QTRY_VERIFY(h.popup()->property("opened").toBool());
    QQuickItem *content = popupContent(h.root);
    QVERIFY(content != nullptr);
    QVERIFY(content->window() != &h.window); // a real, focusable popup window

    // The open network is joined with the keyboard alone.
    QQuickItem *join = itemDescribed(content, QStringLiteral("networkAppletConnectButton"),
                                     QStringLiteral("open network Guest"));
    QVERIFY(join != nullptr);
    QAccessibleInterface *joinInterface = QAccessible::queryAccessibleInterface(join);
    QCOMPARE(joinInterface->role(), QAccessible::Button);
    join->forceActiveFocus();
    QVERIFY(join->hasActiveFocus());
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QTRY_COMPARE(h.transport.operations.size(), 1);
    QCOMPARE(h.transport.operations.last().kind, OperationKind::ConnectVisibleNetwork);
    auto *feedback = h.root->findChild<QQuickItem *>(QStringLiteral("networkAppletFeedback"));
    QVERIFY(feedback == nullptr
            || !feedback->property("text").toString().contains(QStringLiteral("Connected to")));
    h.transport.finishLast(operationResult(OperationKind::ConnectVisibleNetwork,
                                           OperationStatus::Rejected, 20, 1,
                                           QStringLiteral("credentials-required")));
    QTRY_COMPARE(h.controller.requestPhase(), QStringLiteral("failed"));
    QTRY_VERIFY(!h.controller.operationPending());
    QTRY_VERIFY(h.client.operationAdmissionReady());

    // The Wi-Fi switch dispatches, but keeps showing confirmed truth. The
    // Repeater rebuilds rows when truth changes, so re-find it each time.
    const auto wifi = [content]() -> QQuickItem * {
        const auto switches = visualItemsNamed(content, QStringLiteral("networkAppletRadioSwitch"));
        return switches.size() == 1 ? switches.constFirst() : nullptr;
    };
    QTRY_VERIFY(wifi() != nullptr && wifi()->isEnabled());
    QVERIFY(wifi()->property("checked").toBool());
    wifi()->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QTRY_COMPARE(h.transport.operations.size(), 2);
    QCOMPARE(h.transport.operations.last().kind, OperationKind::SetRadio);
    QCOMPARE(h.transport.operations.last().parameters.value(QStringLiteral("enable")).toBool(),
             false);
    QVERIFY(wifi()->property("checked").toBool());
    QVERIFY(!wifi()->isEnabled());
    h.transport.finishLast(operationResult(OperationKind::SetRadio, OperationStatus::Succeeded));
    QVERIFY(wifi()->property("checked").toBool()); // accepted is not confirmed
    Network::Snapshot off = appletSnapshot(2);
    off.radios[0].softwareEnabled = false;
    off.activeConnections.clear();
    off.accessPoints.clear();
    h.transport.setSnapshot(off);
    QTRY_COMPARE_WITH_TIMEOUT(h.controller.requestPhase(), QStringLiteral("succeeded"), 3'000);
    QTRY_VERIFY(wifi() != nullptr && !wifi()->property("checked").toBool());

    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Escape);
    QTRY_VERIFY(!h.popup()->property("opened").toBool());
    QVERIFY(!h.controller.feedbackPresent());
    QCOMPARE(h.transport.operations.size(), 2);
}

void NetworkAppletQmlTests::compactVerticalPanelOpensAccessiblePopup()
{
    Harness h;
    QTRY_COMPARE(h.controller.phase(), QStringLiteral("ready"));
    QString error;
    QVERIFY2(h.load(true, QSize(24, 400), &error), qPrintable(error));
    QTRY_VERIFY(h.window.isExposed());
    QCOMPARE(h.root->width(), 24.0);

    h.openWithKeyboard();
    QTRY_VERIFY(h.popup()->property("opened").toBool());
    QQuickItem *content = popupContent(h.root);
    QVERIFY(content != nullptr);

    const auto rescan = visualItemsNamed(content, QStringLiteral("networkAppletRescanButton"));
    QCOMPARE(rescan.size(), 1);
    QAccessibleInterface *rescanInterface =
        QAccessible::queryAccessibleInterface(rescan.constFirst());
    QCOMPARE(rescanInterface->role(), QAccessible::Button);
    QVERIFY(!rescanInterface->text(QAccessible::Description).isEmpty());
    rescan.constFirst()->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QTRY_COMPARE(h.transport.operations.size(), 1);
    QCOMPARE(h.transport.operations.last().kind, OperationKind::RequestScan);

    const auto heading = visualItemsNamed(content, QStringLiteral("networkAppletHeading"));
    QCOMPARE(heading.size(), 1);
    QCOMPARE(QAccessible::queryAccessibleInterface(heading.constFirst())->role(),
             QAccessible::Heading);
    // "Network Settings…" is absent until the shell injects the route.
    const auto settings = visualItemsNamed(content, QStringLiteral("networkAppletSettingsButton"));
    QCOMPARE(settings.size(), 1);
    QVERIFY(!settings.constFirst()->isVisible());
    int launches = 0;
    h.controller.setSettingsLaunch([&launches] { ++launches; return true; });
    QTRY_VERIFY(settings.constFirst()->isVisible());
    settings.constFirst()->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QTRY_COMPARE(launches, 1);
    QTRY_VERIFY(!h.popup()->property("opened").toBool());
}

QTEST_MAIN(NetworkAppletQmlTests)
#include "tst_network_applet_qml.moc"
