// SPDX-License-Identifier: GPL-3.0-or-later
#include "font_discovery_test_support.h"

#include <QtTest>

using namespace FontDiscoveryTestSupport;
using QindaQt::Services::FontDiscovery::FontDiscoveryLimits;
using QindaQt::Services::FontDiscovery::FontDiscoveryProvider;

class FontDiscoveryBoundsTests final : public QObject {
    Q_OBJECT
private slots:
    void defaultLimitsAreValid();
    void truncationKeepsDeterministicPrefix();
    void manyFilesStayBounded();
};

void FontDiscoveryBoundsTests::defaultLimitsAreValid()
{
    QVERIFY(FontDiscoveryLimits{}.isValid());
    QVERIFY(FontDiscoveryRequest::productionDefault().isWellFormed());
    QVERIFY(FontDiscoveryRequest::productionDefault().configurationFile.isEmpty());
    QVERIFY(FontDiscoveryRequest::productionDefault().fontDirectories.isEmpty());
}

void FontDiscoveryBoundsTests::truncationKeepsDeterministicPrefix()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansLycian-Regular.ttf"),
                          QStringLiteral("NotoSansOgham-Regular.ttf"),
                          QStringLiteral("LiberationMono-Regular.ttf")},
                         stage.fontsPath()));
    QVERIFY(stageConfiguration(stage));

    FontDiscoveryRequest request = requestFor(stage);
    request.limits.maximumFacts = 2;
    const FontDiscoveryResult result = FontDiscoveryProvider(request).discover();
    QVERIFY2(result.available, qPrintable(result.diagnostic));
    QVERIFY(result.truncated);
    QCOMPARE(result.facts.size(), 2);
    QCOMPARE(result.facts.at(0).family, QStringLiteral("Liberation Mono"));
    QCOMPARE(result.facts.at(1).family, QStringLiteral("Noto Sans Lycian"));
}

void FontDiscoveryBoundsTests::manyFilesStayBounded()
{
    // AGENT-NOTE: review finding P2-1 (rejected candidate abc76f3) — the
    // bounded-directory row must stage thousands of entries, not eight.
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(QDir().mkpath(stage.fontsPath()));
    constexpr int FileCount = 3'000;
    for (int i = 0; i < FileCount; ++i) {
        const QString name = QStringLiteral("copy-%1.ttf").arg(i, 4, 10, QLatin1Char('0'));
        QVERIFY(copyFixtureAs(QStringLiteral("NotoSansLycian-Regular.ttf"), name,
                              stage.fontsPath()));
    }
    QVERIFY(stageConfiguration(stage));

    FontDiscoveryRequest request = requestFor(stage);
    const FontDiscoveryResult complete = FontDiscoveryProvider(request).discover();
    QVERIFY2(complete.available, qPrintable(complete.diagnostic));
    QVERIFY(!complete.truncated);
    QCOMPARE(complete.facts.size(), FileCount);
    for (const auto &fact : complete.facts) {
        QCOMPARE(fact.family, QStringLiteral("Noto Sans Lycian"));
    }

    request.limits.maximumFacts = 100;
    const FontDiscoveryResult bounded = FontDiscoveryProvider(request).discover();
    QVERIFY2(bounded.available, qPrintable(bounded.diagnostic));
    QVERIFY(bounded.truncated);
    QCOMPARE(bounded.facts.size(), 100);
    // The truncated result is the deterministic prefix of the complete run.
    QCOMPARE(bounded.facts, complete.facts.sliced(0, 100));
}

QTEST_GUILESS_MAIN(FontDiscoveryBoundsTests)
#include "tst_font_discovery_bounds.moc"
