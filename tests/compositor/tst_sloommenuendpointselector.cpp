// SPDX-License-Identifier: GPL-3.0-or-later

#include "sloommenuendpointselector.h"

#include <QTest>

using namespace QindaQt::Compositor::KWinIntegration;

class SloomMenuEndpointSelectorTest final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void selectsKnownSloomIdentities();
    void preservesKdeAnnouncedEndpointPriority();
    void rejectsNearMissesAndPartialAnnouncements();
};

void SloomMenuEndpointSelectorTest::selectsKnownSloomIdentities()
{
    const auto fromDesktop = selectSloomMenuEndpoint(
        QStringLiteral("/usr/share/applications/sloom-studio.desktop"), {}, {}, {});
    QVERIFY(fromDesktop);
    QCOMPARE(fromDesktop->serviceName, QStringLiteral("org.signalloom.PanelMenu"));
    QCOMPARE(fromDesktop->objectPath, QStringLiteral("/org/signalloom/menus/active"));

    const auto fromWaylandClass = selectSloomMenuEndpoint(
        {}, QStringLiteral("Signal Loom"), {}, {});
    QVERIFY(fromWaylandClass);
    QCOMPARE(*fromWaylandClass, *fromDesktop);

    QVERIFY(selectSloomMenuEndpoint({}, QStringLiteral("studio.sloom.signalloom"), {}, {}));
}

void SloomMenuEndpointSelectorTest::preservesKdeAnnouncedEndpointPriority()
{
    QVERIFY(!selectSloomMenuEndpoint(
        QStringLiteral("sloom-studio"), QStringLiteral("Signal Loom"),
        QStringLiteral("org.example.NativeMenu"), QStringLiteral("/org/example/Menu")));
}

void SloomMenuEndpointSelectorTest::rejectsNearMissesAndPartialAnnouncements()
{
    QVERIFY(!selectSloomMenuEndpoint({}, QStringLiteral("signal-loom-helper"), {}, {}));
    QVERIFY(!selectSloomMenuEndpoint({}, QStringLiteral("other-studio"), {}, {}));
    QVERIFY(!selectSloomMenuEndpoint(
        QStringLiteral("sloom-studio"), {}, QStringLiteral("org.example.Half"), {}));
    QVERIFY(!selectSloomMenuEndpoint(
        QStringLiteral("sloom-studio"), {}, {}, QStringLiteral("/org/example/Half")));
}

QTEST_MAIN(SloomMenuEndpointSelectorTest)
#include "tst_sloommenuendpointselector.moc"
