// SPDX-License-Identifier: GPL-3.0-or-later
#include "font_discovery_test_support.h"

#include "qindaqt/services/font_preferences/font_catalog.h"

#include <QtTest>

using namespace FontDiscoveryTestSupport;
using QindaQt::Services::FontDiscovery::FontDiscoveryProvider;
using QindaQt::Services::FontPreferences::FontCatalog;

class FontDiscoveryTests final : public QObject {
    Q_OBJECT
private slots:
    void discoversVendoredFamiliesDeterministically();
    void mapsWeightSpacingAndIdentity();
    void discoveredFactsProduceValidCatalog();
};

void FontDiscoveryTests::discoversVendoredFamiliesDeterministically()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansLycian-Regular.ttf"),
                          QStringLiteral("NotoSansOgham-Regular.ttf"),
                          QStringLiteral("LiberationMono-Regular.ttf")},
                         stage.fontsPath()));
    QVERIFY(stageConfiguration(stage));

    const FontDiscoveryProvider provider(requestFor(stage));
    const FontDiscoveryResult first = provider.discover();
    QVERIFY2(first.available, qPrintable(first.diagnostic));
    QVERIFY(!first.truncated);
    QCOMPARE(first.facts.size(), 3);

    QStringList families;
    for (const auto &fact : first.facts) {
        QVERIFY(fact.isValid());
        families.append(fact.family);
    }
    // AGENT-CONTRACT: Published order is the provider's explicit sort, never
    // fontconfig cache order.
    QCOMPARE(families, (QStringList{QStringLiteral("Liberation Mono"),
                                    QStringLiteral("Noto Sans Lycian"),
                                    QStringLiteral("Noto Sans Ogham")}));

    const FontDiscoveryResult second = provider.discover();
    QVERIFY(second.available);
    QCOMPARE(second.facts, first.facts);
}

void FontDiscoveryTests::mapsWeightSpacingAndIdentity()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("LiberationMono-Regular.ttf"),
                          QStringLiteral("NotoSansLycian-Regular.ttf")},
                         stage.fontsPath()));
    QVERIFY(stageConfiguration(stage));

    const FontDiscoveryProvider provider(requestFor(stage));
    const FontDiscoveryResult result = provider.discover();
    QVERIFY2(result.available, qPrintable(result.diagnostic));
    QCOMPARE(result.facts.size(), 2);

    const auto &mono = result.facts.at(0);
    QCOMPARE(mono.family, QStringLiteral("Liberation Mono"));
    QVERIFY(mono.isMonospace);
    QVERIFY(mono.isScalable);
    QCOMPARE(mono.weight, 400);
    QVERIFY(!mono.italic);
    QCOMPARE(mono.style, QStringLiteral("Regular"));
    QCOMPARE(mono.postscriptName, QStringLiteral("LiberationMono"));

    const auto &lycian = result.facts.at(1);
    QCOMPARE(lycian.family, QStringLiteral("Noto Sans Lycian"));
    QVERIFY(!lycian.isMonospace);
    QCOMPARE(lycian.weight, 400);
    QCOMPARE(lycian.postscriptName, QStringLiteral("NotoSansLycian-Regular"));
}

void FontDiscoveryTests::discoveredFactsProduceValidCatalog()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansLycian-Regular.ttf"),
                          QStringLiteral("NotoSansOgham-Regular.ttf"),
                          QStringLiteral("LiberationMono-Regular.ttf")},
                         stage.fontsPath()));
    QVERIFY(stageConfiguration(stage));

    const FontDiscoveryProvider provider(requestFor(stage));
    const FontDiscoveryResult result = provider.discover();
    QVERIFY2(result.available, qPrintable(result.diagnostic));

    QString error;
    const FontCatalog catalog = FontCatalog::create(result.facts, &error);
    QVERIFY2(catalog.isValid(), qPrintable(error));
    QCOMPARE(catalog.familyCount(), 3);
    QCOMPARE(catalog.monospaceFamilyNames(), QStringList{QStringLiteral("Liberation Mono")});
    QVERIFY(catalog.proportionalFamilyNames().contains(QStringLiteral("Noto Sans Lycian")));
}

QTEST_GUILESS_MAIN(FontDiscoveryTests)
#include "tst_font_discovery.moc"
