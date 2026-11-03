// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/shell/link_applet/link_applet_presentation.h>

#include "link_applet_test_support.h"

#include <QTest>

using namespace QindaQt::Shell::LinkApplet;
using namespace QindaQt::Shell::LinkApplet::TestSupport;
using QindaQt::Link::LinkDevice;
using QindaQt::Link::parseDevices;

namespace {

ProjectionContext context(const QList<QJsonObject> &devices, bool service = true)
{
    ProjectionContext c;
    c.serviceAvailable = service;
    c.devices = parseDevices(devicesJson(devices));
    c.nowMs = 10'000'000;
    return c;
}

} // namespace

class LinkAppletPresentationTests final : public QObject {
    Q_OBJECT
private slots:
    void absentServiceIsUnavailableEvenWithStaleFacts();
    void emptyListIsNoPhone();
    void connectedShowsBatteryChargingAndNetwork();
    void connectedWithoutStatusHasNoBattery();
    void pairedButNotConnectedIsOfflineWithLastSeen();
    void pairingRequestOutranksEverythingButFormatsTheCode();
    void malformedCodeIsNeverShown();
    void phoneStringsAreSanitized();
    void ringNeedsConnectedAndTheRingCapability();
    void connectedBeatsOfflineAsPrimary();
    void unpairedStrangerIsNotAPairedPhone();
};

void LinkAppletPresentationTests::absentServiceIsUnavailableEvenWithStaleFacts()
{
    auto c = context({deviceObject(QStringLiteral("a"), QStringLiteral("P"), true, true, true, 50)}, false);
    c.pairing = PendingPairing{QStringLiteral("a"), QStringLiteral("P"), QStringLiteral("M"), QStringLiteral("123456")};
    const auto m = projectLinkApplet(c);
    QCOMPARE(toString(m.state), toString(AppletState::Unavailable));
    QVERIFY(!m.hasPairing);
    QVERIFY(m.deviceId.isEmpty());
    QCOMPARE(m.iconColorRole, QStringLiteral("muted"));
}

void LinkAppletPresentationTests::emptyListIsNoPhone()
{
    const auto m = projectLinkApplet(context({}));
    QCOMPARE(toString(m.state), toString(AppletState::NoPhone));
    QCOMPARE(m.unreadCount, 0);
}

void LinkAppletPresentationTests::connectedShowsBatteryChargingAndNetwork()
{
    const auto m = projectLinkApplet(context(
        {deviceObject(QStringLiteral("a"), QStringLiteral("Pixel"), true, true, true, 87, true)}));
    QCOMPARE(toString(m.state), toString(AppletState::Connected));
    QCOMPARE(m.connectionText, QStringLiteral("USB"));
    QCOMPARE(m.batteryPercent, 87);
    QCOMPARE(m.batteryText, QStringLiteral("87%"));
    QVERIFY(m.charging);
    QCOMPARE(m.networkText, QStringLiteral("Wi-Fi, signal 3 of 4"));
    QCOMPARE(m.iconColorRole, QStringLiteral("success"));
    QVERIFY(m.stateText.contains(QStringLiteral("charging")));
}

void LinkAppletPresentationTests::connectedWithoutStatusHasNoBattery()
{
    const auto m = projectLinkApplet(context(
        {deviceObject(QStringLiteral("a"), QStringLiteral("Pixel"), true, true, true)}));
    QCOMPARE(toString(m.state), toString(AppletState::Connected));
    QCOMPARE(m.batteryPercent, -1);
    QVERIFY(m.batteryText.isEmpty());
    QVERIFY(m.networkText.isEmpty());
}

void LinkAppletPresentationTests::pairedButNotConnectedIsOfflineWithLastSeen()
{
    auto c = context({deviceObject(QStringLiteral("a"), QStringLiteral("Pixel"), true, false, false,
                                   -1, false, {}, {}, 10'000'000 - 5 * 60'000)});
    const auto m = projectLinkApplet(c);
    QCOMPARE(toString(m.state), toString(AppletState::Offline));
    QCOMPARE(m.lastSeenText, QStringLiteral("Last seen 5 min ago"));
    QVERIFY(!m.canRing);
}

void LinkAppletPresentationTests::pairingRequestOutranksEverythingButFormatsTheCode()
{
    auto c = context({deviceObject(QStringLiteral("a"), QStringLiteral("Pixel"), true, true, true, 40)});
    c.pairing = PendingPairing{QStringLiteral("b"), QStringLiteral("New phone"), QStringLiteral("SM-X"),
                               QStringLiteral("007123")};
    const auto m = projectLinkApplet(c);
    QCOMPARE(toString(m.state), toString(AppletState::PairingRequest));
    QVERIFY(m.hasPairing);
    QCOMPARE(m.pairing.code, QStringLiteral("007 123"));
    QCOMPARE(m.pairing.deviceId, QStringLiteral("b"));
    QCOMPARE(m.pairing.name, QStringLiteral("New phone"));
    QCOMPARE(m.iconColorRole, QStringLiteral("warning"));
}

void LinkAppletPresentationTests::malformedCodeIsNeverShown()
{
    for (const QString &bad : {QString(), QStringLiteral("12345"), QStringLiteral("1234567"),
                               QStringLiteral("12a456"), QStringLiteral("١٢٣٤٥٦")}) {
        QVERIFY2(formatPairingCode(bad).isEmpty(), qPrintable(bad));
        auto c = context({});
        c.pairing = PendingPairing{QStringLiteral("b"), QStringLiteral("N"), QStringLiteral("M"), bad};
        QCOMPARE(toString(projectLinkApplet(c).state), toString(AppletState::NoPhone));
    }
    QCOMPARE(formatPairingCode(QStringLiteral("123456")), QStringLiteral("123 456"));
}

void LinkAppletPresentationTests::phoneStringsAreSanitized()
{
    auto c = context({});
    c.pairing = PendingPairing{QStringLiteral("b"),
                               QStringLiteral("Evil\n\x1b[31m") + QString(QChar(0x202E)) + QStringLiteral("phone<b>x</b>") + QString(200, QLatin1Char('z')),
                               QStringLiteral("M\tX"), QStringLiteral("123456")};
    const auto m = projectLinkApplet(c);
    QVERIFY(!m.pairing.name.contains(QLatin1Char('\n')));
    QVERIFY(!m.pairing.name.contains(QChar(0x1b)));
    QVERIFY(!m.pairing.name.contains(QChar(0x202E)));
    QVERIFY(m.pairing.name.size() <= 64);
    QVERIFY(m.pairing.name.contains(QStringLiteral("<b>"))); // kept literally; QML renders plain text
    QCOMPARE(m.pairing.model, QStringLiteral("M X"));
}

void LinkAppletPresentationTests::ringNeedsConnectedAndTheRingCapability()
{
    const QStringList caps{QStringLiteral("status"), QStringLiteral("ring")};
    QVERIFY(projectLinkApplet(context({deviceObject(QStringLiteral("a"), QStringLiteral("P"), true, true, true,
                                                    10, false, QStringLiteral("usb"), caps)}))
                .canRing);
    QVERIFY(!projectLinkApplet(context({deviceObject(QStringLiteral("a"), QStringLiteral("P"), true, true, true,
                                                     10, false, QStringLiteral("usb"),
                                                     {QStringLiteral("status")})}))
                 .canRing);
    QVERIFY(!projectLinkApplet(context({deviceObject(QStringLiteral("a"), QStringLiteral("P"), true, false,
                                                     false, -1, false, {}, caps)}))
                 .canRing);
}

void LinkAppletPresentationTests::connectedBeatsOfflineAsPrimary()
{
    const auto m = projectLinkApplet(context(
        {deviceObject(QStringLiteral("old"), QStringLiteral("Old"), true, false, false),
         deviceObject(QStringLiteral("live"), QStringLiteral("Live"), true, true, true, 20)}));
    QCOMPARE(m.deviceId, QStringLiteral("live"));
    QCOMPARE(toString(m.state), toString(AppletState::Connected));
}

void LinkAppletPresentationTests::unpairedStrangerIsNotAPairedPhone()
{
    const auto m = projectLinkApplet(context(
        {deviceObject(QStringLiteral("x"), QStringLiteral("Stranger"), false, true, false)}));
    QCOMPARE(toString(m.state), toString(AppletState::NoPhone));
    QVERIFY(m.deviceId.isEmpty());
}

QTEST_GUILESS_MAIN(LinkAppletPresentationTests)
#include "tst_link_applet_presentation.moc"
