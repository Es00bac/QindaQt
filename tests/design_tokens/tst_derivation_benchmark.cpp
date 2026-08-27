// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/design_tokens/design_tokens.h"
#include "qindaqt/design_tokens/token_deriver.h"
#include "qindaqt/themes/theme_loader.h"

#include <QtTest>

using namespace QindaQt::DesignTokens;
using namespace QindaQt::Themes;

namespace {

constexpr int batchesPerIteration = 1000;
volatile quint64 benchmarkSink = 0;

} // namespace

class DerivationBenchmark final : public QObject {
    Q_OBJECT

private slots:
    void deriveAllFiveBuiltIns();
};

void DerivationBenchmark::deriveAllFiveBuiltIns()
{
    const auto themes = ThemeLoader::fromDirectory(
        QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes"));
    QCOMPARE(themes.size(), 5);
    for (const auto &theme : themes) {
        QVERIFY2(theme.ok, qPrintable(theme.error));
    }

    // AGENT-NOTE: This benchmark records measurements without a CI wall-clock
    // assertion. Shared runners are too noisy for a stable absolute threshold;
    // each timed iteration contains 1000 five-theme batches so sub-millisecond
    // derivation remains visible to QtTest's stable millisecond reporter.
    QBENCHMARK {
        quint64 checksum = 0;
        for (int batch = 0; batch < batchesPerIteration; ++batch) {
            for (const auto &theme : themes) {
                const auto result = DesignTokenDeriver::derive(
                    theme.theme,
                    {.basePointSize = 11.0,
                     .textScale = 1.25,
                     .reducedMotion = true,
                     .reducedTransparency = true,
                     .highContrast = true});
                if (!result.ok()) {
                    qFatal("benchmark derivation failed: %s", qPrintable(result.diagnostic));
                }
                checksum += static_cast<quint64>(result.tokens->motion().base);
            }
        }
        benchmarkSink = checksum;
    }
}

QTEST_GUILESS_MAIN(DerivationBenchmark)
#include "tst_derivation_benchmark.moc"
