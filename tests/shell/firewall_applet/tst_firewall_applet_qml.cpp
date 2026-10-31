// SPDX-License-Identifier: GPL-3.0-or-later
#include "../icon_resolution_test_fixture.h"
#include "desktop_controls_qml_test_support.h"
#include "firewall_applet_controller.h"
#include "firewall_applet_test_support.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_FirewallAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Shell::FirewallApplet;
using namespace QindaQt::Shell::FirewallApplet::TestSupport;
using QindaQt::Firewall::Client::FirewallClient;

namespace {

const QString kAlertId = QStringLiteral("password-guessing:203.0.113.9:1761940000");

QVariantMap testTheme()
{
    return {{QStringLiteral("cornerRadius"), 8}};
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
    auto *popup = root->findChild<QObject *>(QStringLiteral("firewallAppletPopup"));
    return popup ? popup->property("contentItem").value<QQuickItem *>() : nullptr;
}

struct Harness final {
    FakeFirewallTransport transport;
    FirewallClient client;
    FirewallAppletController controller;
    QQmlEngine engine;
    std::unique_ptr<QObject> owned;
    QQuickItem *root = nullptr;
    QQuickWindow window;

    Harness() : client(transport), controller(&client, true) { QVERIFY(client.start()); }

    bool load(QString *error)
    {
        engine.addImportPath(QStringLiteral(QINDAQT_FIREWALL_APPLET_QML_IMPORT_PATH));
        if (!QindaQt::Tests::DesktopControls::publishTokens(engine)) {
            *error = QStringLiteral("token publication failed");
            return false;
        }
        if (!QindaQt::Tests::installResolvedIconFixture(
                engine, QStringLiteral(QINDAQT_FIREWALL_APPLET_ICON_FIXTURE_ROOT),
                {QStringLiteral("security-high-symbolic")}, error)) {
            return false;
        }
        QQmlComponent component(&engine);
        component.loadFromModule(QStringLiteral("QindaQt.Shell.FirewallApplet"),
                                 QStringLiteral("FirewallApplet"));
        if (!component.isReady()) {
            *error = component.errorString();
            return false;
        }
        owned.reset(component.createWithInitialProperties(
            {{QStringLiteral("access"), QVariant::fromValue(&controller)},
             {QStringLiteral("theme"), testTheme()}}));
        root = qobject_cast<QQuickItem *>(owned.get());
        if (root == nullptr) {
            *error = component.errorString();
            return false;
        }
        window.setGeometry(0, 0, 480, 30);
        root->setParentItem(window.contentItem());
        root->setSize(QSizeF(32, 28));
        window.show();
        return true;
    }

    QObject *popup() const
    {
        return root->findChild<QObject *>(QStringLiteral("firewallAppletPopup"));
    }

    void openWithKeyboard()
    {
        auto *summary = root->findChild<QQuickItem *>(QStringLiteral("firewallAppletSummary"));
        summary->forceActiveFocus();
        QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    }
};

} // namespace

class FirewallAppletQmlTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void unavailablePanelHasAccessibleTooltipAndNoBadge();
    void alarmPopupShowsTheTopAlertsPlainEnglishFields();
    void confirmMineButtonCallsConfirmMineWithTopAlertId();
    void dismissButtonCallsDismissWithTopAlertId();
    void openButtonCallsOpenWindowWithTopAlertId();
};

void FirewallAppletQmlTests::unavailablePanelHasAccessibleTooltipAndNoBadge()
{
    Harness h;
    QString error;
    QVERIFY2(h.load(&error), qPrintable(error));
    QTRY_VERIFY(h.window.isExposed());

    auto *summary = h.root->findChild<QQuickItem *>(QStringLiteral("firewallAppletSummary"));
    QVERIFY(summary != nullptr);
    QAccessibleInterface *summaryInterface = QAccessible::queryAccessibleInterface(summary);
    QVERIFY(summaryInterface != nullptr);
    QCOMPARE(summaryInterface->role(), QAccessible::Button);
    QVERIFY(summaryInterface->text(QAccessible::Name)
                .contains(QStringLiteral("Background watching is off")));

    auto *badge = h.root->findChild<QQuickItem *>(QStringLiteral("firewallAppletBadge"));
    QVERIFY(badge == nullptr || !badge->isVisible());
}

namespace {

// Each button test gets its own Harness (its own top-level window and
// popup) rather than reusing one across several open/close cycles: closing
// a QindaTK PanelPopup drops focus to a separate window, and restoring the
// OS-level active window across that boundary is not something this offscreen
// harness needs to prove — the presentation and controller tests already
// cover the state machine exhaustively. One open, one click, one assertion
// keeps each case independent and fast.
QQuickItem *openAlarmPopup(Harness &h, QString *error)
{
    h.transport.setProperties(
        QStringLiteral("live"),
        headlineJson(QStringLiteral("danger"), QStringLiteral("Possible break-in — act now"), {}),
        2,
        alertJson(kAlertId, QStringLiteral("danger"), QStringLiteral("Possible break-in — act now"),
                 QStringLiteral("12 attempts from 203.0.113.9."), QStringLiteral("Yes, dangerous."),
                 QStringLiteral("Block this address."), /*canConfirm=*/true));
    h.transport.announceAvailable(true);
    if (!(h.controller.state() == QStringLiteral("alarm"))) {
        *error = QStringLiteral("controller did not reach the alarm state");
        return nullptr;
    }
    if (!h.load(error)) {
        return nullptr;
    }
    for (int waited = 0; waited < 5'000 && !h.window.isExposed(); waited += 10) {
        QTest::qWait(10);
    }
    if (!h.window.isExposed()) {
        *error = QStringLiteral("window did not expose");
        return nullptr;
    }
    h.openWithKeyboard();
    for (int waited = 0; waited < 5'000 && !h.popup()->property("opened").toBool();
         waited += 10) {
        QTest::qWait(10);
    }
    if (!h.popup()->property("opened").toBool()) {
        *error = QStringLiteral("popup did not open");
        return nullptr;
    }
    return popupContent(h.root);
}

} // namespace

void FirewallAppletQmlTests::alarmPopupShowsTheTopAlertsPlainEnglishFields()
{
    Harness h;
    QString error;
    QQuickItem *content = openAlarmPopup(h, &error);
    QVERIFY2(content != nullptr, qPrintable(error));

    auto *badge = h.root->findChild<QQuickItem *>(QStringLiteral("firewallAppletBadge"));
    QVERIFY(badge != nullptr && badge->isVisible());

    const auto headings = visualItemsNamed(content, QStringLiteral("firewallAppletHeading"));
    QCOMPARE(headings.size(), 1);
    QCOMPARE(QAccessible::queryAccessibleInterface(headings.constFirst())->role(),
             QAccessible::Heading);
    QCOMPARE(headings.constFirst()->property("text").toString(),
             QStringLiteral("Possible break-in — act now"));

    const auto summaryLabels = visualItemsNamed(content, QStringLiteral("firewallAppletSummary"));
    QCOMPARE(summaryLabels.size(), 1);
    QCOMPARE(summaryLabels.constFirst()->property("text").toString(),
             QStringLiteral("12 attempts from 203.0.113.9."));

    const auto meaningLabels = visualItemsNamed(content, QStringLiteral("firewallAppletMeaning"));
    QCOMPARE(meaningLabels.size(), 1);
    QCOMPARE(meaningLabels.constFirst()->property("text").toString(),
             QStringLiteral("Yes, dangerous."));

    const auto adviceLabels = visualItemsNamed(content, QStringLiteral("firewallAppletAdvice"));
    QCOMPARE(adviceLabels.size(), 1);
    QCOMPARE(adviceLabels.constFirst()->property("text").toString(),
             QStringLiteral("Block this address."));
}

void FirewallAppletQmlTests::confirmMineButtonCallsConfirmMineWithTopAlertId()
{
    Harness h;
    QString error;
    QQuickItem *content = openAlarmPopup(h, &error);
    QVERIFY2(content != nullptr, qPrintable(error));

    // "This was me": visible because canConfirm is true, and calls
    // ConfirmMine(topAlert id) through the controller.
    const auto confirmButtons =
        visualItemsNamed(content, QStringLiteral("firewallAppletConfirmMineButton"));
    QCOMPARE(confirmButtons.size(), 1);
    QVERIFY(confirmButtons.constFirst()->isVisible());
    confirmButtons.constFirst()->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QCOMPARE(h.transport.confirmedMine, QStringList{kAlertId});
    QTRY_VERIFY(!h.popup()->property("opened").toBool());
}

void FirewallAppletQmlTests::dismissButtonCallsDismissWithTopAlertId()
{
    Harness h;
    QString error;
    QQuickItem *content = openAlarmPopup(h, &error);
    QVERIFY2(content != nullptr, qPrintable(error));

    // "Got it, hide this": calls Dismiss(topAlert id).
    const auto dismissButtons =
        visualItemsNamed(content, QStringLiteral("firewallAppletDismissButton"));
    QCOMPARE(dismissButtons.size(), 1);
    dismissButtons.constFirst()->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QCOMPARE(h.transport.dismissed, QStringList{kAlertId});
    QTRY_VERIFY(!h.popup()->property("opened").toBool());
}

void FirewallAppletQmlTests::openButtonCallsOpenWindowWithTopAlertId()
{
    Harness h;
    QString error;
    QQuickItem *content = openAlarmPopup(h, &error);
    QVERIFY2(content != nullptr, qPrintable(error));

    // "Open QindaFirewall": calls OpenWindow(topAlert id).
    const auto openButtons = visualItemsNamed(content, QStringLiteral("firewallAppletOpenButton"));
    QCOMPARE(openButtons.size(), 1);
    openButtons.constFirst()->forceActiveFocus();
    QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    QCOMPARE(h.transport.openedWindow, QStringList{kAlertId});
}

QTEST_MAIN(FirewallAppletQmlTests)
#include "tst_firewall_applet_qml.moc"
