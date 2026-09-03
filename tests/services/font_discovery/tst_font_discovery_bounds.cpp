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
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(QDir().mkpath(stage.fontsPath()));
    for (int i = 0; i < 8; ++i) {
        const QString name = QStringLiteral("copy-%1.ttf").arg(i);
        QVERIFY(QFile::copy(QStringLiteral(QINDAQT_FONT_FIXTURES)
                                + QStringLiteral("/NotoSansLycian-Regular.ttf"),
                            stage.fontsPath() + QLatin1Char('/') + name));
    }
    QVERIFY(stageConfiguration(stage));

    FontDiscoveryRequest request = requestFor(stage);
    const FontDiscoveryResult complete = FontDiscoveryProvider(request).discover();
    QVERIFY2(complete.available, qPrintable(complete.diagnostic));
    QCOMPARE(complete.facts.size(), 8);

    request.limits.maximumFacts = 3;
    const FontDiscoveryResult bounded = FontDiscoveryProvider(request).discover();
    QVERIFY2(bounded.available, qPrintable(bounded.diagnostic));
    QVERIFY(bounded.truncated);
    QCOMPARE(bounded.facts.size(), 3);
    for (const auto &fact : bounded.facts) {
        QCOMPARE(fact.family, QStringLiteral("Noto Sans Lycian"));
    }
}

QTEST_GUILESS_MAIN(FontDiscoveryBoundsTests)
#include "tst_font_discovery_bounds.moc"
