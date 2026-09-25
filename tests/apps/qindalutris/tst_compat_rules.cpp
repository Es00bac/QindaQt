// SPDX-License-Identifier: GPL-3.0-or-later
#include <QTest>
#include <QTimeZone>

#include "compat_db.h"

using namespace QindaQt::QindaLutris;

// The compat-db-v1 rules a hostile or careless document must not get past
// (ADR-0275 §3): the environment and winetricks-verb allowlists, pins only on
// tested builds, stamps not in the future, and the byte-level JSON rules
// QJsonDocument alone would wave through. The Python validator is held to the
// same tables in tools/qindalutris-compat/tests/test_rules.py.
namespace {

const QDateTime kNow(QDate(2026, 9, 25), QTime(12, 0), QTimeZone::UTC);

QByteArray document(const QByteArray &game, const QByteArray &generated = "2026-09-01T00:00:00Z",
                    const QByteArray &sources = "[]", const QByteArray &defaultBuild = "") {
  return R"({"schema":"qindalutris-compat-db","version":1,"generated":")" + generated
       + R"(","sources":)" + sources + R"(,"defaults":{"recommendedBuild":")" + defaultBuild
       + R"("},"builds":{"Tested-1":{"status":"tested","notes":""},)"
         R"("Untested-1":{"status":"untested","notes":""}},"games":[)" + game + "]}";
}

QByteArray gameWith(const QByteArray &field) {
  return R"({"id":"g","title":"G","keys":{},)" + field + "}";
}

bool accepted(const QByteArray &bytes) {
  return parseCompatDocument(bytes, kNow).has_value();
}

QByteArray environment(const QString &line) {
  QByteArray escaped = line.toUtf8();
  escaped.replace('\\', "\\\\").replace('"', "\\\"");
  return gameWith(R"("environment":[")" + escaped + R"("])");
}

} // namespace

class tst_compat_rules : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void baselineIsAccepted() {
    QVERIFY(accepted(document(gameWith(R"("notes":["n"])"))));
  }

  void environmentKeysOutsideTheAllowlistRefuseTheDocument_data() {
    QTest::addColumn<QString>("line");
    // The reviewer's list: interpreters, shells, loaders, Wine binaries,
    // Vulkan/GL search paths, container and launcher variables, file/log
    // switches, and case variants of the allowed ones.
    for (const char *key :
         {"PYTHONPATH", "PYTHONHOME", "PYTHONSTARTUP", "BASH_ENV", "ENV", "WINELOADER",
          "WINESERVER", "WINEDLLPATH", "WINEPREFIX", "WINE", "VK_ICD_FILENAMES",
          "VK_ADD_LAYER_PATH", "VK_INSTANCE_LAYERS", "GCONV_PATH", "LIBGL_DRIVERS_PATH",
          "__EGL_VENDOR_LIBRARY_FILENAMES", "GIO_MODULE_DIR", "PRESSURE_VESSEL_FILESYSTEMS_RW",
          "PRESSURE_VESSEL_SHELL", "UMU_ZENITY", "UMU_RUNTIME_UPDATE", "UMU_ID", "PROTONPATH",
          "GAMEID", "STORE", "STEAM_COMPAT_DATA_PATH", "STEAM_COMPAT_MOUNTS", "LD_PRELOAD",
          "LD_LIBRARY_PATH", "ld_preload", "path", "PATH", "HOME", "BROWSER", "PERL5OPT",
          "NODE_OPTIONS", "DXVK_LOG_PATH", "DXVK_LOG_LEVEL", "DXVK_CONFIG_FILE",
          "DXVK_STATE_CACHE_PATH", "VKD3D_SHADER_CACHE_PATH", "VKD3D_LOG_FILE",
          "PROTON_LOG", "PROTON_LOG_DIR", "PROTON_VERB", "PROTON_CRASH_REPORT_DIR",
          "PROTON_ENABLE_NVAPI", "MESA_GLTHREAD", "dxvk_hud", "DXVK_", "__GL_WRITE_TEXT_SECTION",
          "__GL_SHADER_DISK_CACHE_PATH",
          // vkd3d-proton: environment forwarding, and switches that take paths.
          "VKD3D_UNIX_ENV", "VKD3D_UNIX_POST_ENV", "VKD3D_QUEUE_PROFILE", "VKD3D_SHADER_OVERRIDE",
          "VKD3D_QA_HASHES", "VKD3D_SHADER_DUMP_PATH", "VKD3D_PROFILE_PATH", "VKD3D_DEBUG",
          // Not read by upstream DXVK, or debug/path switches.
          "DXVK_ASYNC", "DXVK_FRAME_RATE", "DXVK_DEBUG", "DXVK_SHADER_CACHE_PATH",
          "DXVK_SHADER_DUMP_PATH", "DXVK_CAPTURE_FRAMES"}) {
      QTest::newRow(key) << (QLatin1String(key) + QStringLiteral("=1"));
    }
  }
  void environmentKeysOutsideTheAllowlistRefuseTheDocument() {
    QFETCH(QString, line);
    QVERIFY(!accepted(document(environment(line))));
  }

  void allowlistedEnvironmentIsAccepted_data() {
    QTest::addColumn<QString>("line");
    for (const char *line :
         {"WINEDLLOVERRIDES=locationapi=d;nvapi,nvapi64=d", "DXVK_HUD=fps",
          "DXVK_CONFIG=dxgi.syncInterval = 0", "DXVK_ENABLE_NVAPI=1", "VKD3D_CONFIG=dxr11",
          "VKD3D_FEATURE_LEVEL=12_1", "VKD3D_FRAME_RATE=60", "VKD3D_SWAPCHAIN_LATENCY_FRAMES=1",
          "PROTON_NO_ESYNC=1", "PROTON_NO_WM_DECORATION=1", "PROTON_ENABLE_WAYLAND=1",
          "PROTON_FORCE_NVAPI=1", "WINE_FULLSCREEN_FSR=1", "WINE_FULLSCREEN_FSR_STRENGTH=2",
          "RADV_PERFTEST=gpl", "mesa_glthread=true", "STAGING_SHARED_MEMORY=1",
          "__GL_SHADER_DISK_CACHE=1", "__GL_THREADED_OPTIMIZATIONS=1", "DXVK_HUD="}) {
      QTest::newRow(line) << QString::fromLatin1(line);
    }
  }
  void allowlistedEnvironmentIsAccepted() {
    QFETCH(QString, line);
    QVERIFY(accepted(document(environment(line))));
  }

  void environmentValuesCannotExpandOrRepeat() {
    QVERIFY(!accepted(document(environment(QStringLiteral("DXVK_HUD=$HOME")))));
    QVERIFY(!accepted(document(environment(QStringLiteral("DXVK_HUD=`id`")))));
    QVERIFY(!accepted(document(environment(QStringLiteral("DXVK_HUD=a\u0001")))));
    QVERIFY(!accepted(document(gameWith(R"("environment":["DXVK_HUD=1","DXVK_HUD=0"])"))));
    QVERIFY(accepted(document(gameWith(R"("environment":["DXVK_CONFIG=a","DXVK_HUD=0"])"))));
    const QString longest = QStringLiteral("DXVK_HUD=") + QString(1089 - 9, QLatin1Char('x'));
    QVERIFY(accepted(document(environment(longest))));
    QVERIFY(!accepted(document(environment(longest + QLatin1Char('x')))));
  }

  void winetricksVerbsMustBeInTheCommittedAllowlist_data() {
    QTest::addColumn<QString>("verb");
    QTest::addColumn<bool>("ok");
    const auto row = [](const char *verb, bool ok) {
      QTest::newRow(verb) << QString::fromLatin1(verb) << ok;
    };
    row("corefonts", true);
    row("win10", true);
    row("d3dcompiler_47", true);
    row("renderer=vulkan", true);
    row("vd=off", true);
    row("mimeassoc=off", true);
    row("mimeassoc=on", false); // Wine file associations can reach the host desktop
    row("remove_mono", false);
    row("annihilate", false);
    row("-q", false);
    row("--self-update", false);
    row("prefix=evil", false);
    row("arch=win32", false);
    row("list-all", false);
    row("list", false);
    row("apps", false);
    row("7zip", false);      // apps category: installs a program
    row("steam", false);     // apps category
    row("bad", false);       // winetricks' failing test verb
    row("winver=", false);
    row("notaverb", false);
    row("Corefonts", false);
  }
  void winetricksVerbsMustBeInTheCommittedAllowlist() {
    QFETCH(QString, verb);
    QFETCH(bool, ok);
    QCOMPARE(accepted(document(gameWith(R"("winetricks":[")" + verb.toUtf8() + R"("])"))), ok);
  }

  void pinsMustNameTestedBuilds() {
    QVERIFY(accepted(document(gameWith(R"("proton":{"recommended":"Tested-1"})"))));
    QVERIFY(!accepted(document(gameWith(R"("proton":{"recommended":"Untested-1"})"))));
    QVERIFY(!accepted(document(gameWith(R"("proton":{"recommended":"Unknown-1"})"))));
    QVERIFY(accepted(document(gameWith(R"("notes":["n"])"), "2026-09-01T00:00:00Z", "[]",
                              "Tested-1")));
    QVERIFY(!accepted(document(gameWith(R"("notes":["n"])"), "2026-09-01T00:00:00Z", "[]",
                               "Untested-1")));
    // Avoiding an untested or unknown build is fine: a warning needs no test.
    QVERIFY(accepted(document(gameWith(
        R"("proton":{"avoid":[{"build":"Unknown-2","reason":"r","source":"s"}]})"))));
  }

  void stampsMoreThanADayAheadAreRefused() {
    const QByteArray game = gameWith(R"("notes":["n"])");
    QVERIFY(accepted(document(game, "2026-09-26T12:00:00Z")));  // exactly +24 h
    QVERIFY(!accepted(document(game, "2026-09-26T12:00:01Z")));
    const QByteArray future = R"([{"id":"s","url":"https://s.org/","retrieved":"2026-09-27T00:00:00Z"}])";
    const QByteArray fine = R"([{"id":"s","url":"https://s.org/","retrieved":"2026-09-26T00:00:00Z"}])";
    QVERIFY(!accepted(document(game, "2026-09-01T00:00:00Z", future)));
    QVERIFY(accepted(document(game, "2026-09-01T00:00:00Z", fine)));
  }

  void chooseNewerNeverPrefersAFutureStamp() {
    // Loaded under a later clock, then judged under kNow.
    const QDateTime later = kNow.addDays(30);
    std::optional<CompatDocument> ahead =
        parseCompatDocument(document(gameWith(R"("notes":["n"])"), "2026-10-20T00:00:00Z"), later);
    std::optional<CompatDocument> current =
        parseCompatDocument(document(gameWith(R"("notes":["n"])"), "2026-09-20T00:00:00Z"), kNow);
    QVERIFY(ahead.has_value() && current.has_value());
    const CompatDatabase aheadDb = *CompatDatabase::fromDocument(*ahead);
    const CompatDatabase currentDb = *CompatDatabase::fromDocument(*current);
    QCOMPARE(chooseNewer(currentDb, aheadDb, kNow).generated(), currentDb.generated());
    QCOMPARE(chooseNewer(aheadDb, currentDb, kNow).generated(), currentDb.generated());
    QVERIFY(!chooseNewer(aheadDb, CompatDatabase(), kNow).isLoaded());
    QCOMPARE(chooseNewer(currentDb, aheadDb, later).generated(), aheadDb.generated());
  }

  void byteLevelJsonRulesMatchPython_data() {
    QTest::addColumn<QByteArray>("bytes");
    QTest::addColumn<bool>("ok");
    const QByteArray good = document(gameWith(R"("notes":["n"])"));
    QByteArray text = good;
    QTest::newRow("baseline") << good << true;
    QTest::newRow("utf-8 BOM") << (QByteArray("\xEF\xBB\xBF") + good) << false;
    QTest::newRow("version 1.") << QByteArray(good).replace("\"version\":1", "\"version\":1.") << false;
    QTest::newRow("version .1e1") << QByteArray(good).replace("\"version\":1", "\"version\":.1e1") << false;
    QTest::newRow("version 1.0") << QByteArray(good).replace("\"version\":1", "\"version\":1.0") << true;
    QTest::newRow("version 01") << QByteArray(good).replace("\"version\":1", "\"version\":01") << false;
    QTest::newRow("duplicate key") << QByteArray(good).replace(
        "\"version\":1", "\"version\":1,\"version\":1") << false;
    QTest::newRow("escaped duplicate key") << QByteArray(good).replace(
        "\"version\":1", "\"version\":1,\"v\\u0065rsion\":1") << false;
    QTest::newRow("raw tab in string") << QByteArray(good).replace("\"n\"", "\"a\tb\"") << false;
    QTest::newRow("form feed whitespace") << QByteArray(good).replace(",\"sources\"", ",\f\"sources\"") << false;
    QTest::newRow("invalid utf-8") << QByteArray(good).replace("\"n\"", "\"\xFF\"") << false;
    QTest::newRow("overlong utf-8") << QByteArray(good).replace("\"n\"", "\"\xC0\x80\"") << false;
    QTest::newRow("encoded surrogate") << QByteArray(good).replace("\"n\"", "\"\xED\xA0\x80\"") << false;
    QTest::newRow("trailing garbage") << (good + " x") << false;
    QTest::newRow("trailing comma") << QByteArray(good).replace("\"notes\":[\"n\"]", "\"notes\":[\"n\",]") << false;
    QTest::newRow("single quotes") << QByteArray(good).replace("\"n\"", "'n'") << false;
    QTest::newRow("deep nesting") << (good.chopped(1) + ",\"x\":" + QByteArray(600, '[')
                                      + QByteArray(600, ']') + "}") << false;
  }
  void byteLevelJsonRulesMatchPython() {
    QFETCH(QByteArray, bytes);
    QFETCH(bool, ok);
    QCOMPARE(accepted(bytes), ok);
  }
};

QTEST_GUILESS_MAIN(tst_compat_rules)
#include "tst_compat_rules.moc"
