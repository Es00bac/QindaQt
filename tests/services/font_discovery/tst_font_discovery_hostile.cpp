// SPDX-License-Identifier: GPL-3.0-or-later
#include "font_discovery_test_support.h"

#include <QtTest>

using namespace FontDiscoveryTestSupport;
using QindaQt::Services::FontDiscovery::FontDiscoveryProvider;

class FontDiscoveryHostileTests final : public QObject {
    Q_OBJECT
private slots:
    void malformedConfigurationIsUnavailable();
    void missingConfigurationIsUnavailable();
    void missingInjectedDirectoryIsUnavailable();
    void malformedRequestsAreUnavailable();
    void emptyDirectoryIsAvailableButEmpty();
    void hostileDirectoryContentIsSkipped();
    void overLongStringsRejectThePattern();
};

void FontDiscoveryHostileTests::malformedConfigurationIsUnavailable()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QFile file(stage.configPath());
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("<fontconfig><dir></fontconfig>\n");
    file.close();

    const FontDiscoveryResult result = FontDiscoveryProvider(requestFor(stage)).discover();
    QVERIFY(!result.available);
    QVERIFY(result.facts.isEmpty());
    QVERIFY(!result.diagnostic.isEmpty());
}

void FontDiscoveryHostileTests::missingConfigurationIsUnavailable()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansLycian-Regular.ttf")}, stage.fontsPath()));

    const FontDiscoveryResult result = FontDiscoveryProvider(requestFor(stage)).discover();
    QVERIFY(!result.available);
    QVERIFY(result.facts.isEmpty());
}

void FontDiscoveryHostileTests::missingInjectedDirectoryIsUnavailable()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(stageConfiguration(stage));

    FontDiscoveryRequest request;
    request.configurationFile = stage.configPath();
    request.fontDirectories = {stage.root.filePath(QStringLiteral("does-not-exist"))};
    const FontDiscoveryResult result = FontDiscoveryProvider(request).discover();
    // AGENT-GUARD: An injected directory that cannot be used is fail-closed,
    // never silently dropped into a partial discovery.
    QVERIFY(!result.available);
    QVERIFY(result.facts.isEmpty());
}

void FontDiscoveryHostileTests::malformedRequestsAreUnavailable()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(stageConfiguration(stage));

    FontDiscoveryRequest blankDirectory = requestFor(stage);
    blankDirectory.fontDirectories.append(QStringLiteral("   "));
    QVERIFY(!FontDiscoveryProvider(blankDirectory).discover().available);

    FontDiscoveryRequest invalidLimits = requestFor(stage);
    invalidLimits.limits.maximumFacts = 0;
    QVERIFY(!FontDiscoveryProvider(invalidLimits).discover().available);

    FontDiscoveryRequest hugeLimits = requestFor(stage);
    hugeLimits.limits.maximumFacts = 100'000;
    QVERIFY(!FontDiscoveryProvider(hugeLimits).discover().available);
}

void FontDiscoveryHostileTests::emptyDirectoryIsAvailableButEmpty()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(QDir().mkpath(stage.fontsPath()));
    QVERIFY(stageConfiguration(stage));

    const FontDiscoveryResult result = FontDiscoveryProvider(requestFor(stage)).discover();
    QVERIFY2(result.available, qPrintable(result.diagnostic));
    QVERIFY(!result.truncated);
    QVERIFY(result.facts.isEmpty());
}

void FontDiscoveryHostileTests::hostileDirectoryContentIsSkipped()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansOgham-Regular.ttf")}, stage.fontsPath()));
    QVERIFY(stageConfiguration(stage));

    QFile fakeFont(stage.fontsPath() + QStringLiteral("/evil.ttf"));
    QVERIFY(fakeFont.open(QIODevice::WriteOnly | QIODevice::Truncate));
    fakeFont.write("<html>this is not a font</html>\n");
    fakeFont.close();
    QFile fakeConfig(stage.fontsPath() + QStringLiteral("/fonts.conf"));
    QVERIFY(fakeConfig.open(QIODevice::WriteOnly | QIODevice::Truncate));
    fakeConfig.write("<fontconfig/>\n");
    fakeConfig.close();

    const FontDiscoveryResult result = FontDiscoveryProvider(requestFor(stage)).discover();
    QVERIFY2(result.available, qPrintable(result.diagnostic));
    QCOMPARE(result.facts.size(), 1);
    QCOMPARE(result.facts.first().family, QStringLiteral("Noto Sans Ogham"));
    QVERIFY(result.facts.first().isValid());
}

void FontDiscoveryHostileTests::overLongStringsRejectThePattern()
{
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansLycian-Regular.ttf")}, stage.fontsPath()));
    QVERIFY(stageConfiguration(stage));

    FontDiscoveryRequest request = requestFor(stage);
    request.limits.maximumStringBytes = 4;
    const FontDiscoveryResult result = FontDiscoveryProvider(request).discover();
    QVERIFY2(result.available, qPrintable(result.diagnostic));
    // "Noto Sans Lycian" exceeds the bound, so its pattern is rejected
    // fail-closed instead of truncated into a fabricated family identity.
    QVERIFY(result.facts.isEmpty());
}

QTEST_GUILESS_MAIN(FontDiscoveryHostileTests)
#include "tst_font_discovery_hostile.moc"
