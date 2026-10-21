// SPDX-License-Identifier: GPL-3.0-or-later

// Strip racks under client-stream churn (audio-service wiki, 2026-10-21).
// The worker re-applies every declaration on each graph event. A rack that
// PipeWire refused was loaded again on every one of them, and PipeWire 1.6.8
// leaks the filter-graph nodes it had already loaded on each refusal: every
// short playback client grew the service heap by hundreds of kilobytes. Each
// row churns pw-cat clients through a private PipeWire/WirePlumber with the
// real backend and coordinator in this process, and bounds the heap growth.

#include "support/private_audio_churn.h"

#include <QtCore/QScopeGuard>
#include <QtCore/QTemporaryDir>
#include <QtTest>

using namespace QindaQt::Audio;
using namespace QindaQt::Audio::Test;

namespace
{

constexpr int kWarmupCycles = 10;
constexpr int kMeasuredCycles = 40;
// The leak was 48-110 kB per refused load, about three loads per stream per
// racked strip: over 600 kB per stream here. A correct worker measured a few
// hundred bytes per stream. The bound sits far from both.
constexpr qint64 kMaxGrowthPerCycle = 8 * 1024;
const QString kRackRefused = QStringLiteral("PipeWire refused the rack of");

// Both racked strips get the same rack: the hardware strip reads the fixture
// input, the virtual strip its own sink's monitor.
bool declareRacks(PrivateAudioChurn &churn, const StripProcessing &rack)
{
    for (const QString &strip :
         {QStringLiteral("strip.hw.1"), QStringLiteral("strip.virtual.1")}) {
        OperationRequest request;
        request.kind = OperationKind::SetStripProcessing;
        request.consoleId = strip;
        request.processing = rack;
        if (!churn.apply(request)) {
            return false;
        }
    }
    return true;
}

QString growthMessage(const qint64 growth)
{
    return QStringLiteral("heap grew %1 bytes over %2 client streams (%3 per stream, bound %4)")
        .arg(growth)
        .arg(kMeasuredCycles)
        .arg(growth / kMeasuredCycles)
        .arg(kMaxGrowthPerCycle);
}

} // namespace

class WirePlumberRackChurnTests final : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void anEqualizerRackLoadsAndStreamChurnKeepsTheHeapBounded();
    void aRefusedRackIsNotRetriedOnEveryGraphEvent();
};

void WirePlumberRackChurnTests::initTestCase()
{
#if !defined(__GLIBC__)
    QSKIP("the bound is measured with glibc's mallinfo2");
#endif
}

// The equalizer is PipeWire's builtin biquads, so this rack needs no plugin
// package. It was refused on every graph event ("unknown input port
// eq_low:Input") and leaked the loaded biquads each time.
void WirePlumberRackChurnTests::anEqualizerRackLoadsAndStreamChurnKeepsTheHeapBounded()
{
    PrivateAudioChurn churn;
    QString failure;
    QVERIFY2(churn.start(&failure), qPrintable(failure));
    WarningTally warnings({kRackRefused});
    StripProcessing rack;
    rack.equalizer.enabled = true;
    QVERIFY(declareRacks(churn, rack));
    // Heap first: on the old worker the refusal storm is the failure under
    // test, and it shows before the absent processed sources do.
    churn.settle();
    const std::optional<qint64> growth = churn.churnGrowth(kWarmupCycles, kMeasuredCycles);
    QVERIFY2(growth.has_value(), "a pw-cat client never appeared or never left");
    qInfo().noquote() << growthMessage(*growth);
    QVERIFY2(*growth < kMaxGrowthPerCycle * kMeasuredCycles, qPrintable(growthMessage(*growth)));
    QVERIFY(hasInput(churn.snapshot(), QStringLiteral("qindaqt.console.strip.hw.1.processed")));
    QVERIFY(hasInput(churn.snapshot(),
                     QStringLiteral("qindaqt.console.strip.virtual.1.processed")));
    QCOMPARE(warnings.count(kRackRefused), 0);
}

// A rack PipeWire must refuse whatever the plugin set: an empty LADSPA path
// removes the limiter. The builtin equalizer ahead of it is loaded before
// the refusal, which is the leaking case; the worker must try it once.
void WirePlumberRackChurnTests::aRefusedRackIsNotRetriedOnEveryGraphEvent()
{
    // In-process filter-graph reads LADSPA_PATH at each plugin load. It is
    // set before the backend thread exists and restored after the rig (and
    // that thread) is gone: setenv must not race the worker's getenv.
    QTemporaryDir noPlugins(QStringLiteral("/tmp/qindaqt-audio-no-ladspa-XXXXXX"));
    QVERIFY(noPlugins.isValid());
    const QByteArray previousPath = qgetenv("LADSPA_PATH");
    const bool hadPath = qEnvironmentVariableIsSet("LADSPA_PATH");
    qputenv("LADSPA_PATH", noPlugins.path().toUtf8());
    const auto restorePath = qScopeGuard([&] {
        if (hadPath) {
            qputenv("LADSPA_PATH", previousPath);
        } else {
            qunsetenv("LADSPA_PATH");
        }
    });
    PrivateAudioChurn churn;
    QString failure;
    QVERIFY2(churn.start(&failure), qPrintable(failure));
    WarningTally warnings({kRackRefused});
    StripProcessing rack;
    rack.equalizer.enabled = true;
    rack.limiter.enabled = true;
    QVERIFY(declareRacks(churn, rack));
    churn.settle();
    const std::optional<qint64> growth = churn.churnGrowth(kWarmupCycles, kMeasuredCycles);
    QVERIFY2(growth.has_value(), "a pw-cat client never appeared or never left");
    qInfo().noquote() << growthMessage(*growth);
    QVERIFY2(*growth < kMaxGrowthPerCycle * kMeasuredCycles, qPrintable(growthMessage(*growth)));
    // The scenario really was a refusal: neither strip gained a processed
    // source, and each refusal reached the journal exactly once.
    QVERIFY(!hasInput(churn.snapshot(), QStringLiteral("qindaqt.console.strip.hw.1.processed")));
    QVERIFY(!hasInput(churn.snapshot(),
                      QStringLiteral("qindaqt.console.strip.virtual.1.processed")));
    QCOMPARE(warnings.count(kRackRefused), 2);
}

QTEST_GUILESS_MAIN(WirePlumberRackChurnTests)
#include "tst_wireplumber_rack_churn.moc"
