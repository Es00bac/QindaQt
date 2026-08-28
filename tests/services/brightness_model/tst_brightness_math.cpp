// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/brightness_model/brightness_math.h>

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtTest>

using namespace QindaQt::Brightness;
namespace Power = QindaQt::Power;

class BrightnessMathTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void mapsEndpointsAndNonzeroMinimum();
  void rejectsInvalidRangesAndValues();
  void remainsMonotonicAcrossNormalizedDomain();
  void rawRoundTripStaysWithinQuantizationBound();
};

void BrightnessMathTests::mapsEndpointsAndNonzeroMinimum() {
  QCOMPARE(normalizeRaw(10, 110, 10).value, 0U);
  QCOMPARE(normalizeRaw(10, 110, 60).value, 5'000U);
  QCOMPARE(normalizeRaw(10, 110, 110).value,
           Power::kNormalizedBrightnessMaximum);
  QCOMPARE(denormalizeRaw(10, 110, 0).value, 10U);
  QCOMPARE(denormalizeRaw(10, 110, 5'000).value, 60U);
  QCOMPARE(denormalizeRaw(10, 110, 10'000).value, 110U);
}

void BrightnessMathTests::rejectsInvalidRangesAndValues() {
  QCOMPARE(normalizeRaw(10, 10, 10).error, MathError::InvalidRange);
  QCOMPARE(normalizeRaw(20, 10, 15).error, MathError::InvalidRange);
  QCOMPARE(normalizeRaw(0, Power::kMaximumRawBrightness + 1U, 0).error,
           MathError::InvalidRange);
  QCOMPARE(normalizeRaw(10, 20, 9).error, MathError::ValueOutOfRange);
  QCOMPARE(normalizeRaw(10, 20, 21).error, MathError::ValueOutOfRange);
  QCOMPARE(denormalizeRaw(10, 20, 10'001).error, MathError::ValueOutOfRange);
}

void BrightnessMathTests::remainsMonotonicAcrossNormalizedDomain() {
  quint32 previous = 0;
  for (quint32 normalized = 0;
       normalized <= Power::kNormalizedBrightnessMaximum; ++normalized) {
    const RawResult result =
        denormalizeRaw(17, Power::kMaximumRawBrightness, normalized);
    QVERIFY(result.succeeded());
    QVERIFY(result.value >= previous);
    previous = result.value;
  }
  QCOMPARE(previous, Power::kMaximumRawBrightness);
}

void BrightnessMathTests::rawRoundTripStaysWithinQuantizationBound() {
  constexpr quint32 minimum = 23;
  constexpr quint32 maximum = Power::kMaximumRawBrightness;
  constexpr quint64 range = static_cast<quint64>(maximum) - minimum;
  constexpr quint64 tolerance =
      (range + 2U * Power::kNormalizedBrightnessMaximum - 1U) /
      (2U * Power::kNormalizedBrightnessMaximum);
  const QList<quint32> samples = {minimum,      minimum + 1U, 123'456U,
                                  500'000'000U, maximum - 1U, maximum};
  for (const quint32 sample : samples) {
    const NormalizedResult normalized = normalizeRaw(minimum, maximum, sample);
    QVERIFY(normalized.succeeded());
    const RawResult restored =
        denormalizeRaw(minimum, maximum, normalized.value);
    QVERIFY(restored.succeeded());
    const quint64 difference = restored.value > sample
                                   ? restored.value - sample
                                   : sample - restored.value;
    QVERIFY2(difference <= tolerance, qPrintable(QString::number(difference)));
  }
}

QTEST_GUILESS_MAIN(BrightnessMathTests)
#include "tst_brightness_math.moc"
