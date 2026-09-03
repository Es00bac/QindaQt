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
    void injectedDirectoryWithoutConfigurationIsRejected();
    void emptyDirectoryIsAvailableButEmpty();
    void hostileDirectoryContentIsSkipped();
    void symlinkLoopAndUnreadableFilesAreSkipped();
    void controlCharactersInStyleRejectThePattern();
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

void FontDiscoveryHostileTests::injectedDirectoryWithoutConfigurationIsRejected()
{
    // AGENT-NOTE: review finding P1-2 (rejected candidate abc76f3) — a request
    // with an injected directory but no configuration file reached
    // FcInitLoadConfig() on the unrepaired tree and enumerated thousands of
    // ambient host facts. It must be ill-formed and fail closed. The ambient
    // trap below (HOME carrying its own fontconfig configuration and fonts)
    // would surface the fixture family if ambient state were ever consulted.
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansLycian-Regular.ttf")}, stage.fontsPath()));

    const QString homePath = stage.root.filePath(QStringLiteral("home"));
    const QString ambientConfigDir = homePath + QStringLiteral("/.config/fontconfig");
    QVERIFY(QDir().mkpath(ambientConfigDir));
    QVERIFY(writeConfigurationWithDirectories(ambientConfigDir + QStringLiteral("/fonts.conf"),
                                              stage.cachePath(), {stage.fontsPath()}));
    QVERIFY(QDir().mkpath(homePath + QStringLiteral("/.fonts")));
    QVERIFY(copyFixtureAs(QStringLiteral("NotoSansOgham-Regular.ttf"),
                          QStringLiteral("NotoSansOgham-Regular.ttf"),
                          homePath + QStringLiteral("/.fonts")));

    const EnvGuard home("HOME", homePath.toUtf8());
    const EnvGuard configFile("FONTCONFIG_FILE", "/dev/null");
    const EnvGuard configPath("FONTCONFIG_PATH", "/nonexistent");

    FontDiscoveryRequest request;
    request.configurationFile.clear();
    request.fontDirectories = {stage.fontsPath()};
    QVERIFY(!request.isWellFormed());

    const FontDiscoveryResult result = FontDiscoveryProvider(request).discover();
    QVERIFY(!result.available);
    QVERIFY(result.facts.isEmpty());
    QVERIFY(!result.diagnostic.isEmpty());
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

void FontDiscoveryHostileTests::symlinkLoopAndUnreadableFilesAreSkipped()
{
    // AGENT-NOTE: review finding P2-1 (rejected candidate abc76f3) — the
    // hostile-directory row must cover a symlink loop and an unreadable file,
    // not only a broken font file. Completion of discover() is itself the
    // loop-safety proof (the ctest row carries a TIMEOUT bound).
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansLycian-Regular.ttf"),
                          QStringLiteral("NotoSansOgham-Regular.ttf")},
                         stage.fontsPath()));
    QVERIFY(stageConfiguration(stage));

    QFile broken(stage.fontsPath() + QStringLiteral("/broken.ttf"));
    QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
    broken.write("\x00\x01\x02 not a font\n");
    broken.close();

    QVERIFY(QFile::link(QStringLiteral("."), stage.fontsPath() + QStringLiteral("/loop")));

    const QString unreadablePath = stage.fontsPath() + QStringLiteral("/unreadable.ttf");
    QVERIFY(copyFixtureAs(QStringLiteral("LiberationMono-Regular.ttf"),
                          QStringLiteral("unreadable.ttf"), stage.fontsPath()));
    QVERIFY(QFile::setPermissions(unreadablePath, QFile::Permissions{}));

    const FontDiscoveryResult result = FontDiscoveryProvider(requestFor(stage)).discover();
    QVERIFY2(result.available, qPrintable(result.diagnostic));
    // The unreadable copy is Liberation Mono bytes; whether or not the
    // process may read it, the published family set is exactly the staged
    // fixture families.
    QSet<QString> families;
    for (const auto &fact : result.facts) {
        QVERIFY(fact.isValid());
        families.insert(fact.family);
    }
    QVERIFY(families.contains(QStringLiteral("Noto Sans Lycian")));
    QVERIFY(families.contains(QStringLiteral("Noto Sans Ogham")));
    QVERIFY(!families.contains(QStringLiteral("broken")));

    QVERIFY(QFile::setPermissions(unreadablePath, QFile::ReadOwner | QFile::WriteOwner));
}

void FontDiscoveryHostileTests::controlCharactersInStyleRejectThePattern()
{
    // AGENT-NOTE: review finding P1-3 (rejected candidate abc76f3) — a
    // fontconfig scan rule assigning a newline-containing FC_STYLE published
    // the control characters verbatim on the unrepaired tree because only
    // FC_FAMILY was validated. Every discovered string must be
    // control-character-free before publication; the whole pattern is
    // rejected instead.
    Stage stage;
    QVERIFY(stage.root.isValid());
    QVERIFY(copyFixtures({QStringLiteral("NotoSansLycian-Regular.ttf")}, stage.fontsPath()));

    QFile config(stage.configPath());
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray fixturePath =
        QFileInfo(stage.fontsPath() + QStringLiteral("/NotoSansLycian-Regular.ttf"))
            .absoluteFilePath()
            .toUtf8();
    const QByteArray xml = QByteArrayLiteral("<?xml version=\"1.0\"?>\n<fontconfig><cachedir>")
                           + stage.cachePath().toUtf8()
                           + QByteArrayLiteral("</cachedir>"
                                               "<match target=\"scan\"><test name=\"file\"><string>")
                           + fixturePath
                           + QByteArrayLiteral("</string></test><edit name=\"style\" mode=\"assign\">"
                                               "<string>Bad\nStyle</string></edit></match></fontconfig>\n");
    QCOMPARE(config.write(xml), xml.size());
    config.close();

    const FontDiscoveryResult result = FontDiscoveryProvider(requestFor(stage)).discover();
    QVERIFY2(result.available, qPrintable(result.diagnostic));
    QVERIFY(result.facts.isEmpty());
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
