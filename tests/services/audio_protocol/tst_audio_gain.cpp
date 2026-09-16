// SPDX-License-Identifier: LGPL-3.0-or-later

// ADR-0171: one gain law shared by the service, every console fader and the
// routing matrix. These rows pin the properties a mixing console depends on -
// exact silence at the bottom stop, a round-trippable fader taper, and a
// findable unity position.

#include <qindaqt/services/audio_protocol/audio_gain.h>

#include <QtTest>

#include <cmath>

using namespace QindaQt::Audio;

class AudioGainTests final : public QObject
{
    Q_OBJECT
private slots:
    void decibelsMapToAmplitudeAtTheKnownPoints();
    void theBottomStopIsExactlySilent();
    void nonFiniteAndOutOfRangeInputClampInsteadOfPropagating();
    void theFaderTaperRoundTripsWithoutDrift();
    void theTaperIsMonotonicAndPutsUnityInTheWorkingRange();
};

void AudioGainTests::decibelsMapToAmplitudeAtTheKnownPoints()
{
    QVERIFY(qFuzzyCompare(linearFromGainDb(kUnityGainDb), 1.0));
    // -6.0206 dB is half amplitude; +6.0206 dB is double.
    QVERIFY(std::abs(linearFromGainDb(-6.0206) - 0.5) < 1e-4);
    QVERIFY(std::abs(linearFromGainDb(6.0206) - 2.0) < 1e-3);
    QVERIFY(std::abs(linearFromGainDb(-20.0) - 0.1) < 1e-9);

    // Inverse agrees on the same points.
    QVERIFY(std::abs(gainDbFromLinear(1.0) - kUnityGainDb) < 1e-9);
    QVERIFY(std::abs(gainDbFromLinear(0.5) + 6.0206) < 1e-3);
    QVERIFY(std::abs(gainDbFromLinear(0.1) + 20.0) < 1e-9);
}

// A fader at its bottom stop must actually turn the source off. 10^(-60/20) is
// 0.001, which stays audible on a loud source and reads as a broken fader.
void AudioGainTests::theBottomStopIsExactlySilent()
{
    QCOMPARE(linearFromGainDb(kMinGainDb), 0.0);
    QCOMPARE(linearFromGainDb(-100.0), 0.0);
    QCOMPARE(gainDbFromFaderPosition(0.0), kMinGainDb);
    QCOMPARE(linearFromGainDb(gainDbFromFaderPosition(0.0)), 0.0);
    // Silence round-trips to the bottom of the scale, never to -infinity.
    QCOMPARE(gainDbFromLinear(0.0), kMinGainDb);
    QCOMPARE(gainDbFromLinear(-1.0), kMinGainDb);
}

void AudioGainTests::nonFiniteAndOutOfRangeInputClampInsteadOfPropagating()
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    QCOMPARE(clampGainDb(nan), kMinGainDb);
    QCOMPARE(clampGainDb(inf), kMaxGainDb);
    QCOMPARE(clampGainDb(-inf), kMinGainDb);
    QCOMPARE(clampGainDb(1000.0), kMaxGainDb);
    QCOMPARE(gainDbFromLinear(nan), kMinGainDb);
    QCOMPARE(gainDbFromFaderPosition(nan), kMinGainDb);
    QCOMPARE(gainDbFromFaderPosition(-1.0), kMinGainDb);
    QCOMPARE(gainDbFromFaderPosition(2.0), kMaxGainDb);
    QVERIFY(std::isfinite(linearFromGainDb(nan)));
}

// Dragging a fader and reading the value back must not drift.
void AudioGainTests::theFaderTaperRoundTripsWithoutDrift()
{
    for (int step = 0; step <= 1000; ++step) {
        const double position = double(step) / 1000.0;
        const double back = faderPositionFromGainDb(gainDbFromFaderPosition(position));
        QVERIFY2(std::abs(back - position) < 1e-9,
                 qPrintable(QStringLiteral("position %1 returned %2")
                                .arg(position).arg(back)));
    }
    // ... and the same in the other direction across the printed scale.
    for (int db = int(kMinGainDb); db <= int(kMaxGainDb); ++db) {
        const double back = gainDbFromFaderPosition(faderPositionFromGainDb(db));
        QVERIFY2(std::abs(back - double(db)) < 1e-9,
                 qPrintable(QStringLiteral("%1 dB returned %2").arg(db).arg(back)));
    }
}

void AudioGainTests::theTaperIsMonotonicAndPutsUnityInTheWorkingRange()
{
    double previous = -1e9;
    for (int step = 0; step <= 200; ++step) {
        const double value = gainDbFromFaderPosition(double(step) / 200.0);
        QVERIFY(value >= previous);
        previous = value;
    }
    QCOMPARE(gainDbFromFaderPosition(1.0), kMaxGainDb);

    // Unity is findable and sits high on the travel, as on a real console -
    // not at the midpoint a linear-in-dB scale would give it.
    const double unity = unityFaderPosition();
    QVERIFY(unity > 0.6);
    QVERIFY(unity < 0.95);
    QVERIFY(std::abs(gainDbFromFaderPosition(unity) - kUnityGainDb) < 1e-9);
}

QTEST_APPLESS_MAIN(AudioGainTests)
#include "tst_audio_gain.moc"
