// SPDX-License-Identifier: GPL-3.0-or-later
// The console dock's meters and the Audio1 meter lease behind them (ADR-0394),
// driven without an OBS frontend or a running libobs: the lease is its own
// collaborator (DockMeterLease) over the real AudioClient on a fake transport.
// Qt first: libobs's SIMDe native aliases collide with the compiler intrinsics
// that Qt's qsimd.h includes afterwards.
#include <QApplication>
#include <QImage>
#include <QSignalSpy>
#include <QtTest>

#include "console_dock.h"
#include "dock_meter_lease.h"
#include "support/fake_audio_transport.h"

#include <memory>

using namespace QindaQt;
using namespace QindaQt::ObsBridge;
using namespace QindaQt::Tests;

class ObsConsoleDockTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void dockAnnouncesWhenItsMetersAreVisible();
    void theLeaseFollowsTheDockVisibility();
    void aDockAlreadyOnScreenTakesTheLeaseAtOnce();
    void destroyingTheLeaseGivesItBack();
    void destroyingTheDockGivesTheLeaseBack();
    void aLeaseAndAnotherSurfaceShareOneSubscription();
    void anEmptyBatchIdlesTheBars();
};

namespace
{

DesiredSource consoleSource(const QString &id)
{
    DesiredSource source;
    source.consoleId = id;
    source.sourceName = id;
    return source;
}

// The meter bar paints a green fill for a quiet peak and nothing but dark grey
// when idle, so the dock's pixels say whether a reading is on screen without
// reaching into its private widgets.
bool showsGreenFill(QWidget &widget)
{
    const QImage image = widget.grab().toImage();
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) == QColor(80, 190, 110)) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

void ObsConsoleDockTests::dockAnnouncesWhenItsMetersAreVisible()
{
    ConsoleDock dock;
    QSignalSpy visibility(&dock, &ConsoleDock::meterVisibilityChanged);
    dock.show();
    QTRY_COMPARE(visibility.size(), 1);
    QVERIFY(visibility.constLast().constFirst().toBool());
    dock.hide();
    QTRY_COMPARE(visibility.size(), 2);
    QVERIFY(!visibility.constLast().constFirst().toBool());
}

void ObsConsoleDockTests::theLeaseFollowsTheDockVisibility()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    ConsoleDock dock;
    DockMeterLease lease(&client, &dock);
    // A dock that is closed, tabbed away or on a hidden window shows no meter.
    QCOMPARE(client.levelsLeaseCount(), 0);
    QVERIFY(!lease.held());
    QVERIFY(transport.levelsRequests.isEmpty());

    dock.show();
    QCOMPARE(client.levelsLeaseCount(), 1);
    QVERIFY(lease.held());
    dock.show(); // idempotent: one dock, one lease
    QCOMPARE(client.levelsLeaseCount(), 1);
    dock.hide();
    QCOMPARE(client.levelsLeaseCount(), 0);
    QVERIFY(!lease.held());
    dock.show();
    QCOMPARE(client.levelsLeaseCount(), 1);
    dock.hide();
    QCOMPARE(client.levelsLeaseCount(), 0);
    // The stream was switched on and off exactly with the dock, no more often.
    QCOMPARE(transport.levelsRequests, (QList<bool>{true, false, true, false}));
}

void ObsConsoleDockTests::aDockAlreadyOnScreenTakesTheLeaseAtOnce()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    ConsoleDock dock;
    dock.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dock));
    DockMeterLease lease(&client, &dock);
    QCOMPARE(client.levelsLeaseCount(), 1);
    QVERIFY(lease.held());
    QCOMPARE(transport.levelsRequests, (QList<bool>{true}));
}

void ObsConsoleDockTests::destroyingTheLeaseGivesItBack()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    ConsoleDock dock;
    {
        DockMeterLease lease(&client, &dock);
        dock.show();
        QCOMPARE(client.levelsLeaseCount(), 1);
    }
    // The bridge stopped: the subscription is gone, and a late visibility change
    // on the dock cannot take a lease on a client nobody holds it for.
    QCOMPARE(client.levelsLeaseCount(), 0);
    QCOMPARE(transport.levelsRequests, (QList<bool>{true, false}));
    dock.hide();
    dock.show();
    QCOMPARE(client.levelsLeaseCount(), 0);
}

void ObsConsoleDockTests::destroyingTheDockGivesTheLeaseBack()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    auto dock = std::make_unique<ConsoleDock>();
    DockMeterLease lease(&client, dock.get());
    dock->show();
    QCOMPARE(client.levelsLeaseCount(), 1);
    // OBS owns a registered dock and may destroy it before the bridge stops.
    dock.reset();
    QCOMPARE(client.levelsLeaseCount(), 0);
    QVERIFY(!lease.held());
    QCOMPARE(transport.levelsRequests, (QList<bool>{true, false}));
}

void ObsConsoleDockTests::aLeaseAndAnotherSurfaceShareOneSubscription()
{
    FakeAudioTransport transport;
    Audio::AudioClient client(&transport);
    ConsoleDock dock;
    DockMeterLease lease(&client, &dock);
    // Another meter in the same process (a second dock, a test probe) holds a
    // lease of its own: the subscription is on while either is.
    client.retainLevels();
    dock.show();
    QCOMPARE(client.levelsLeaseCount(), 2);
    dock.hide();
    QCOMPARE(client.levelsLeaseCount(), 1);
    QCOMPARE(transport.levelsRequests, (QList<bool>{true}));
    client.releaseLevels();
    QCOMPARE(transport.levelsRequests, (QList<bool>{true, false}));
}

void ObsConsoleDockTests::anEmptyBatchIdlesTheBars()
{
    ConsoleDock dock;
    dock.setSources({consoleSource(QStringLiteral("strip.hardware.1")),
                     consoleSource(QStringLiteral("strip.hardware.2"))});
    QCOMPARE(dock.rowCount(), 2);
    dock.resize(420, 200);
    dock.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dock));
    QVERIFY(!showsGreenFill(dock));

    Audio::LevelReading heard;
    heard.id = QStringLiteral("strip.hardware.1");
    heard.level.known = true;
    heard.level.peakDb = -30.0;
    heard.level.rmsDb = -40.0;
    dock.setLevels({heard});
    QVERIFY(showsGreenFill(dock));

    // The empty batch the client sends when the last lease is released (and the
    // service sends when metering stops) must not leave a frozen reading.
    dock.setLevels({});
    QVERIFY(!showsGreenFill(dock));
    QCOMPARE(dock.rowCount(), 2);
    // A fresh frame afterwards still lands.
    dock.setLevels({heard});
    QVERIFY(showsGreenFill(dock));
}

QTEST_MAIN(ObsConsoleDockTests)
#include "tst_obs_console_dock.moc"
