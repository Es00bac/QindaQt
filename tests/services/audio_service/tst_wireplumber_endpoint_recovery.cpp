// SPDX-License-Identifier: GPL-3.0-or-later

// A virtual strip's sink that the daemon destroys comes back without a
// reconnect (audio-service wiki, 2026-10-21). The sink is made through a raw
// proxy, and that proxy's record is what stops a second copy being created
// while the first is still being announced. The record used to outlive the
// node, so after a `pw-cli destroy` the strip had no sink, and applications
// no device to play into, until the service reconnected.

#include "support/private_audio_churn.h"

#include <QtTest>

#include <optional>

using namespace QindaQt::Audio;
using namespace QindaQt::Audio::Test;

namespace
{

const QString kStripSink = QStringLiteral("qindaqt.console.strip.virtual.1");

std::optional<Device> outputNamed(const Snapshot &snapshot, const QString &nodeName)
{
    for (const Device &device : snapshot.outputs) {
        if (device.nodeName == nodeName) {
            return device;
        }
    }
    return std::nullopt;
}

qsizetype outputsNamed(const Snapshot &snapshot, const QString &nodeName)
{
    qsizetype count = 0;
    for (const Device &device : snapshot.outputs) {
        count += device.nodeName == nodeName ? 1 : 0;
    }
    return count;
}

bool stripBoundTo(const Snapshot &snapshot, const Handle &sink)
{
    for (const Strip &strip : snapshot.console.strips) {
        if (strip.id == QLatin1String("strip.virtual.1")) {
            return strip.sourceKnown && strip.sourceEpoch == sink.epoch
                && strip.sourceSerial == sink.serial;
        }
    }
    return false;
}

} // namespace

class WirePlumberEndpointRecoveryTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void aDestroyedVirtualStripSinkComesBackWithoutAReconnect();
};

void WirePlumberEndpointRecoveryTests::aDestroyedVirtualStripSinkComesBackWithoutAReconnect()
{
    PrivateAudioChurn churn;
    QString failure;
    QVERIFY2(churn.start(&failure), qPrintable(failure));
    const quint64 epoch = churn.snapshot().epoch;
    const std::optional<Device> original = outputNamed(churn.snapshot(), kStripSink);
    QVERIFY(original.has_value());
    quint64 serial = original->handle.serial;

    // Twice, spaced past the recovery interval: each destruction is answered.
    for (int round = 0; round < 2; ++round) {
        QVERIFY2(churn.runTool(QStringLiteral(QINDAQT_PW_CLI_EXECUTABLE),
                               {QStringLiteral("destroy"), kStripSink}),
                 "pw-cli could not destroy the strip sink");
        QVERIFY2(waitFor([&] {
                     const std::optional<Device> now = outputNamed(churn.snapshot(), kStripSink);
                     return now.has_value() && now->handle.serial != serial;
                 }, 10000),
                 qPrintable(QStringLiteral("round %1: the destroyed sink never came back")
                                .arg(round + 1)));
        const Snapshot recovered = churn.snapshot();
        QCOMPARE(recovered.epoch, epoch); // no reconnect, no new authority
        QCOMPARE(outputsNamed(recovered, kStripSink), qsizetype(1));
        const Handle sink = outputNamed(recovered, kStripSink)->handle;
        QVERIFY2(waitFor([&] { return stripBoundTo(churn.snapshot(), sink); }, 5000),
                 "the virtual strip did not follow its new sink");
        serial = sink.serial;
        QTest::qWait(2500);
    }
    churn.settle();
    QCOMPARE(outputsNamed(churn.snapshot(), kStripSink), qsizetype(1));
}

QTEST_GUILESS_MAIN(WirePlumberEndpointRecoveryTests)
#include "tst_wireplumber_endpoint_recovery.moc"
