// SPDX-License-Identifier: GPL-3.0-or-later
#include "../icon_resolution_test_fixture.h"
#include "desktop_controls_qml_test_support.h"
#include "link_applet_controller.h"
#include "link_applet_test_support.h"

#include <QAccessible>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtTest>

#include <memory>

Q_IMPORT_QML_PLUGIN(QindaQt_Shell_LinkAppletPlugin)
Q_IMPORT_QML_PLUGIN(QindaQt_Shell_IconsPlugin)

using namespace QindaQt::Shell::LinkApplet;
using namespace QindaQt::Shell::LinkApplet::TestSupport;
using QindaQt::Link::LinkClient;

namespace {

const QString kId = QStringLiteral("aa");

QList<QQuickItem *> itemsNamed(QQuickItem *root, const QString &name)
{
    QList<QQuickItem *> matches;
    if (root->objectName() == name) matches.append(root);
    for (QQuickItem *child : root->childItems()) matches.append(itemsNamed(child, name));
    return matches;
}

struct Harness final {
    FakeLinkTransport transport;
    LinkClient client;
    LinkAppletController controller;
    QQmlEngine engine;
    std::unique_ptr<QObject> owned;
    QQuickItem *root = nullptr;
    QQuickWindow window;

    Harness() : client(transport), controller(&client, true, true) { QVERIFY(client.start()); }

    bool load(QString *error)
    {
        engine.addImportPath(QStringLiteral(QINDAQT_LINK_APPLET_QML_IMPORT_PATH));
        if (!QindaQt::Tests::DesktopControls::publishTokens(engine)) {
            *error = QStringLiteral("token publication failed");
            return false;
        }
        if (!QindaQt::Tests::installResolvedIconFixture(
                engine, QStringLiteral(QINDAQT_LINK_APPLET_ICON_FIXTURE_ROOT),
                {QStringLiteral("phone-symbolic")}, error)) {
            return false;
        }
        QQmlComponent component(&engine);
        component.loadFromModule(QStringLiteral("QindaQt.Shell.LinkApplet"),
                                 QStringLiteral("LinkApplet"));
        if (!component.isReady()) {
            *error = component.errorString();
            return false;
        }
        owned.reset(component.createWithInitialProperties(
            {{QStringLiteral("access"), QVariant::fromValue(&controller)},
             {QStringLiteral("theme"), QVariantMap{{QStringLiteral("cornerRadius"), 8}}}}));
        root = qobject_cast<QQuickItem *>(owned.get());
        if (root == nullptr) {
            *error = component.errorString();
            return false;
        }
        window.setGeometry(0, 0, 480, 30);
        root->setParentItem(window.contentItem());
        root->setSize(QSizeF(64, 28));
        window.show();
        for (int waited = 0; waited < 5'000 && !window.isExposed(); waited += 10) {
            QTest::qWait(10);
        }
        return window.isExposed();
    }

    QObject *popup() const { return root->findChild<QObject *>(QStringLiteral("linkAppletPopup")); }
    QQuickItem *content() const
    {
        return popup() ? popup()->property("contentItem").value<QQuickItem *>() : nullptr;
    }
    QQuickItem *named(const QString &name) const
    {
        const auto found = itemsNamed(content(), name);
        return found.isEmpty() ? nullptr : found.constFirst();
    }

    bool openWithKeyboard(QString *error)
    {
        auto *summary = root->findChild<QQuickItem *>(QStringLiteral("linkAppletSummary"));
        summary->forceActiveFocus();
        QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
        for (int waited = 0; waited < 5'000 && !popup()->property("opened").toBool(); waited += 10) {
            QTest::qWait(10);
        }
        if (!popup()->property("opened").toBool()) {
            *error = QStringLiteral("popup did not open");
        }
        return popup()->property("opened").toBool();
    }

    void press(QQuickItem *button)
    {
        button->forceActiveFocus();
        QTest::keyClick(QGuiApplication::focusWindow(), Qt::Key_Space);
    }
};

void presentPairing(Harness &h)
{
    h.transport.devicesJson = devicesJson({deviceObject(QStringLiteral("bb"), QStringLiteral("New"),
                                                        false, true, false)});
    h.transport.announceAvailable(true);
    h.transport.raisePairing(QStringLiteral("bb"), QStringLiteral("Pixel <b>9</b>"),
                             QStringLiteral("SM-N"), QStringLiteral("012345"));
}

} // namespace

class LinkAppletQmlTests final : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void unavailableChipIsAccessibleWithoutBadgeOrBattery();
    void connectedChipShowsBatteryAndTheSmsBadgeSlotStaysHidden();
    void pairingPopupShowsCodeAndOpensOnReject();
    void acceptByKeyboardConfirmsTheRequest();
    void rejectByKeyboardRefusesTheRequest();
    void ringIsDisabledWithoutTheCapabilityAndUnpairAsksFirst();
};

void LinkAppletQmlTests::unavailableChipIsAccessibleWithoutBadgeOrBattery()
{
    Harness h;
    QString error;
    QVERIFY2(h.load(&error), qPrintable(error));
    auto *summary = h.root->findChild<QQuickItem *>(QStringLiteral("linkAppletSummary"));
    QVERIFY(summary != nullptr);
    auto *iface = QAccessible::queryAccessibleInterface(summary);
    QVERIFY(iface != nullptr);
    QCOMPARE(iface->role(), QAccessible::Button);
    QVERIFY(iface->text(QAccessible::Name).contains(QStringLiteral("QindaLink is not running")));
    auto *badge = h.root->findChild<QQuickItem *>(QStringLiteral("linkAppletBadge"));
    QVERIFY(badge == nullptr || !badge->isVisible());
    auto *battery = h.root->findChild<QQuickItem *>(QStringLiteral("linkAppletBatteryLabel"));
    QVERIFY(battery == nullptr || !battery->isVisible());
}

void LinkAppletQmlTests::connectedChipShowsBatteryAndTheSmsBadgeSlotStaysHidden()
{
    Harness h;
    h.transport.devicesJson = devicesJson({deviceObject(kId, QStringLiteral("Pixel"), true, true, true, 87, true)});
    h.transport.announceAvailable(true);
    QString error;
    QVERIFY2(h.load(&error), qPrintable(error));
    auto *battery = h.root->findChild<QQuickItem *>(QStringLiteral("linkAppletBatteryLabel"));
    QVERIFY(battery != nullptr && battery->isVisible());
    QVERIFY(battery->property("text").toString().startsWith(QStringLiteral("87%")));
    auto *badge = h.root->findChild<QQuickItem *>(QStringLiteral("linkAppletBadge"));
    QVERIFY(badge != nullptr && !badge->isVisible());
}

void LinkAppletQmlTests::pairingPopupShowsCodeAndOpensOnReject()
{
    Harness h;
    presentPairing(h);
    QString error;
    QVERIFY2(h.load(&error), qPrintable(error));
    QVERIFY2(h.openWithKeyboard(&error), qPrintable(error));

    auto *code = h.named(QStringLiteral("linkAppletPairingCode"));
    QVERIFY(code != nullptr);
    QCOMPARE(code->property("text").toString(), QStringLiteral("012 345"));
    auto *phone = h.named(QStringLiteral("linkAppletPairingPhone"));
    QVERIFY(phone != nullptr);
    QCOMPARE(phone->property("textFormat").toInt(), int(Qt::PlainText));
    QVERIFY(phone->property("text").toString().contains(QStringLiteral("<b>9</b>")));
    auto *accept = h.named(QStringLiteral("linkAppletAcceptButton"));
    auto *reject = h.named(QStringLiteral("linkAppletRejectButton"));
    QVERIFY(accept != nullptr && reject != nullptr);
    QTRY_VERIFY(reject->hasActiveFocus());
    QVERIFY(!accept->hasActiveFocus());
}

void LinkAppletQmlTests::acceptByKeyboardConfirmsTheRequest()
{
    Harness h;
    presentPairing(h);
    QString error;
    QVERIFY2(h.load(&error), qPrintable(error));
    QVERIFY2(h.openWithKeyboard(&error), qPrintable(error));
    h.press(h.named(QStringLiteral("linkAppletAcceptButton")));
    QCOMPARE(h.transport.confirmations, QStringList{QStringLiteral("bb:accept")});
    QTRY_VERIFY(!h.popup()->property("opened").toBool());
}

void LinkAppletQmlTests::rejectByKeyboardRefusesTheRequest()
{
    Harness h;
    presentPairing(h);
    QString error;
    QVERIFY2(h.load(&error), qPrintable(error));
    QVERIFY2(h.openWithKeyboard(&error), qPrintable(error));
    h.press(h.named(QStringLiteral("linkAppletRejectButton")));
    QCOMPARE(h.transport.confirmations, QStringList{QStringLiteral("bb:reject")});
}

void LinkAppletQmlTests::ringIsDisabledWithoutTheCapabilityAndUnpairAsksFirst()
{
    Harness h;
    h.transport.devicesJson = devicesJson({deviceObject(kId, QStringLiteral("Pixel"), true, true, true, 50)});
    h.transport.announceAvailable(true);
    QString error;
    QVERIFY2(h.load(&error), qPrintable(error));
    QVERIFY2(h.openWithKeyboard(&error), qPrintable(error));

    auto *ring = h.named(QStringLiteral("linkAppletRingButton"));
    QVERIFY(ring != nullptr && ring->isVisible());
    QVERIFY(!ring->isEnabled());
    // Open QindaLink is hidden: no launcher seam was injected.
    auto *open = h.named(QStringLiteral("linkAppletOpenButton"));
    QVERIFY(open == nullptr || !open->isVisible());

    h.press(h.named(QStringLiteral("linkAppletUnpairButton")));
    QVERIFY(h.transport.unpaired.isEmpty()); // only asks
    auto *confirm = h.named(QStringLiteral("linkAppletConfirmUnpairButton"));
    QVERIFY(confirm != nullptr);
    QTRY_VERIFY(confirm->isVisible());
    h.press(confirm);
    QCOMPARE(h.transport.unpaired, QStringList{kId});
}

QTEST_MAIN(LinkAppletQmlTests)
#include "tst_link_applet_qml.moc"
