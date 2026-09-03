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
    void reversedFixtureFilenamesProduceIdenticalFacts();
    void productionDefaultUsesOnlyTheDefaultConfiguration();
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

void FontDiscoveryTests::reversedFixtureFilenamesProduceIdenticalFacts()
{
    // AGENT-NOTE: review finding P2-1 (rejected candidate abc76f3) — the
    // determinism row must stage the same fixture bytes under reversed
    // filenames; directory enumeration order must never leak into the
    // published, explicitly sorted facts.
    Stage stageA;
    Stage stageB;
    QVERIFY(stageA.root.isValid());
    QVERIFY(stageB.root.isValid());

    const QStringList fixtures{QStringLiteral("NotoSansLycian-Regular.ttf"),
                               QStringLiteral("NotoSansOgham-Regular.ttf"),
                               QStringLiteral("LiberationMono-Regular.ttf")};
    const QStringList forwardNames{QStringLiteral("01-a.ttf"), QStringLiteral("02-b.ttf"),
                                   QStringLiteral("03-c.ttf")};
    const QStringList reversedNames{QStringLiteral("03-c.ttf"), QStringLiteral("02-b.ttf"),
                                    QStringLiteral("01-a.ttf")};
    for (int i = 0; i < fixtures.size(); ++i) {
        QVERIFY(copyFixtureAs(fixtures.at(i), forwardNames.at(i), stageA.fontsPath()));
        QVERIFY(copyFixtureAs(fixtures.at(i), reversedNames.at(i), stageB.fontsPath()));
    }
    QVERIFY(stageConfiguration(stageA));
    QVERIFY(stageConfiguration(stageB));

    const FontDiscoveryResult first = FontDiscoveryProvider(requestFor(stageA)).discover();
    const FontDiscoveryResult second = FontDiscoveryProvider(requestFor(stageB)).discover();
    QVERIFY2(first.available, qPrintable(first.diagnostic));
    QVERIFY2(second.available, qPrintable(second.diagnostic));
    QCOMPARE(first.facts.size(), 3);
    QCOMPARE(second.facts, first.facts);
}

void FontDiscoveryTests::productionDefaultUsesOnlyTheDefaultConfiguration()
{
    // AGENT-CONTRACT: productionDefault() is the only request shape allowed to
    // resolve the default fontconfig configuration (review finding P1-2). With
    // FONTCONFIG_FILE aimed at a staged configuration, the production shape
    // must find exactly the staged family and nothing ambient.
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("LiberationMono-Regular.ttf")}, stage.fontsPath()));
    QVERIFY(writeConfigurationWithDirectories(stage.configPath(), stage.cachePath(),
                                              {stage.fontsPath()}));

    // The trap HOME carries its own fontconfig configuration and fonts; the
    // staged FONTCONFIG_FILE does not include either, so they must not leak.
    const QString homePath = stage.root.filePath(QStringLiteral("home"));
    QVERIFY(QDir().mkpath(homePath + QStringLiteral("/.config/fontconfig")));
    QVERIFY(writeConfigurationWithDirectories(
        homePath + QStringLiteral("/.config/fontconfig/fonts.conf"), stage.cachePath(),
        {stage.fontsPath()}));

    const EnvGuard home("HOME", homePath.toUtf8());
    const EnvGuard configFile("FONTCONFIG_FILE", stage.configPath().toUtf8());
    const EnvGuard configPath("FONTCONFIG_PATH", "/nonexistent");
    const FontDiscoveryResult staged =
        FontDiscoveryProvider(FontDiscoveryRequest::productionDefault()).discover();
    QVERIFY2(staged.available, qPrintable(staged.diagnostic));
    QCOMPARE(staged.facts.size(), 1);
    QCOMPARE(staged.facts.first().family, QStringLiteral("Liberation Mono"));
}

QTEST_GUILESS_MAIN(FontDiscoveryTests)
#include "tst_font_discovery.moc"
