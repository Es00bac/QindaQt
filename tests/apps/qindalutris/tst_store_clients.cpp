// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

#include "store_clients.h"

using namespace QindaQt::QindaLutris;

// ADR-0275 section 8: the store clients' sign-in addresses, code capture,
// argv and output formats. Fixtures follow what legendary 0.20.34, gogdl
// 1.3.0 and nile 1.2.0 print (signed-out outputs recorded on 2026-09-25;
// signed-in shapes from their source). No client is run.
class tst_store_clients : public QObject {
  Q_OBJECT

  static StoreClientBinaries binaries() {
    return {QStringLiteral("/usr/bin/legendary"), QStringLiteral("/usr/bin/gogdl"),
            QStringLiteral("/usr/bin/nile")};
  }
  static QString root() { return QStringLiteral("/home/u/.config/qindalutris/stores"); }

private Q_SLOTS:
  void ids() {
    for (StoreClient client : {StoreClient::Epic, StoreClient::Gog, StoreClient::Amazon}) {
      QCOMPARE(storeClientForId(storeClientId(client)), client);
    }
    QCOMPARE(storeClientId(StoreClient::Epic), QStringLiteral("egs"));
    QVERIFY(!storeClientForId(QStringLiteral("steam")).has_value());
  }

  void environmentKeepsClientsPrivate() {
    const QHash<QString, QString> env = storeClientEnvironment(root());
    QCOMPARE(env.value(QStringLiteral("LEGENDARY_CONFIG_PATH")), root() + "/legendary");
    QCOMPARE(env.value(QStringLiteral("GOGDL_CONFIG_PATH")), root());
    QCOMPARE(env.value(QStringLiteral("NILE_CONFIG_PATH")), root());
    const StoreClientRun run = accountStatusRun(StoreClient::Epic, binaries(), root());
    QCOMPARE(run.spec.environment, env);
    QVERIFY(run.spec.unsetEnvironmentPrefixes.contains(QStringLiteral("PYTHON")));
    QVERIFY(run.spec.unsetEnvironment.contains(QStringLiteral("LD_PRELOAD")));
    QVERIFY(run.spec.timeoutMs > 0);
  }

  void signInAddresses() {
    const QUrl epic = epicSignInUrl();
    QCOMPARE(epic.host(), QStringLiteral("www.epicgames.com"));
    const QString redirect =
        QUrlQuery(epic).queryItemValue(QStringLiteral("redirectUrl"), QUrl::FullyDecoded);
    QCOMPARE(redirect, QStringLiteral("https://www.epicgames.com/id/api/redirect?clientId="
                                      "34a02cf8f4414e29b15921876da36f9a&responseType=code"));
    const QUrl gog = gogSignInUrl();
    QCOMPARE(gog.host(), QStringLiteral("auth.gog.com"));
    QCOMPARE(QUrlQuery(gog).queryItemValue(QStringLiteral("redirect_uri"), QUrl::FullyDecoded),
             QStringLiteral("https://embed.gog.com/on_login_success?origin=client"));
  }

  void finishedAddresses() {
    QVERIFY(isSignInFinishedUrl(StoreClient::Epic,
                                QUrl(QStringLiteral("https://www.epicgames.com/id/api/redirect?clientId=x"))));
    QVERIFY(!isSignInFinishedUrl(StoreClient::Epic,
                                 QUrl(QStringLiteral("https://www.epicgames.com/id/login"))));
    QVERIFY(isSignInFinishedUrl(
        StoreClient::Gog,
        QUrl(QStringLiteral("https://embed.gog.com/on_login_success?origin=client&code=abcdefgh123"))));
    QVERIFY(!isSignInFinishedUrl(StoreClient::Gog,
                                 QUrl(QStringLiteral("http://embed.gog.com/on_login_success?code=x"))));
    QVERIFY(!isSignInFinishedUrl(
        StoreClient::Gog, QUrl(QStringLiteral("https://evil.example/on_login_success?code=abc"))));
    QVERIFY(isSignInFinishedUrl(
        StoreClient::Amazon,
        QUrl(QStringLiteral("https://www.amazon.com/?openid.oa2.authorization_code=ANabcdefgh"))));
  }

  void codeCapture() {
    const QString epicPage = QStringLiteral(
        "{\"warning\":\"Do not share this code with any 3rd party service. It allows full access "
        "to your Epic account.\",\"redirectUrl\":\"https://localhost/launcher/"
        "authorized?code=0123456789abcdef0123456789abcdef\",\"authorizationCode\":"
        "\"0123456789abcdef0123456789abcdef\",\"exchangeCode\":null,\"sid\":null}");
    QCOMPARE(captureSignInCode(StoreClient::Epic, epicPage),
             QStringLiteral("0123456789abcdef0123456789abcdef"));
    QCOMPARE(captureSignInCode(StoreClient::Epic, QStringLiteral(" 0123456789abcdef0123456789abcdef\n")),
             QStringLiteral("0123456789abcdef0123456789abcdef"));
    QCOMPARE(captureSignInCode(
                 StoreClient::Gog,
                 QStringLiteral("https://embed.gog.com/on_login_success?origin=client&code=Ab-_9xYz01234")),
             QStringLiteral("Ab-_9xYz01234"));
    QCOMPARE(captureSignInCode(StoreClient::Amazon,
                               QStringLiteral("https://www.amazon.com/?openid.assoc_handle=x&"
                                              "openid.oa2.authorization_code=ANzYxWvUtSrQ")),
             QStringLiteral("ANzYxWvUtSrQ"));
    QVERIFY(!captureSignInCode(StoreClient::Gog, QStringLiteral("hello world")).has_value());
    QVERIFY(!captureSignInCode(StoreClient::Epic, QStringLiteral("{\"authorizationCode\":\"; rm\"}"))
                 .has_value());
    QVERIFY(!captureSignInCode(StoreClient::Amazon, QString()).has_value());
  }

  void amazonStart() {
    const QByteArray out =
        "{\"client_id\": \"" + QByteArray(94, 'a') + "\", \"code_verifier\": "
        "\"Zm9vYmFyYmF6cXV4LWZvb2Jhcl9iYXpxdXgtZm9vYmFy\", \"serial\": \""
        + QByteArray(32, 'f') + "\", \"url\": \"https://amazon.com/ap/signin?openid.ns=x\"}\n";
    const auto start = parseAmazonSignInStart(out);
    QVERIFY(start.has_value());
    QCOMPARE(start->url.host(), QStringLiteral("amazon.com"));
    QVERIFY(!parseAmazonSignInStart("{\"url\": \"https://evil.example/\"}").has_value());
    const StoreClientRun run =
        signInCompleteRun(StoreClient::Amazon, binaries(), root(), QStringLiteral("ANcode1234"), start);
    QVERIFY(run.carriesSecrets);
    QCOMPARE(run.spec.arguments,
             QStringList({QStringLiteral("register"), QStringLiteral("--code"),
                          QStringLiteral("ANcode1234"), QStringLiteral("--client-id"),
                          start->clientId, QStringLiteral("--code-verifier"), start->codeVerifier,
                          QStringLiteral("--serial"), start->serial}));
  }

  void completionArgv() {
    StoreClientRun run =
        signInCompleteRun(StoreClient::Epic, binaries(), root(), QStringLiteral("c0de1234"), {});
    QCOMPARE(run.spec.program, QStringLiteral("/usr/bin/legendary"));
    QCOMPARE(run.spec.arguments, QStringList({QStringLiteral("auth"), QStringLiteral("--code"),
                                              QStringLiteral("c0de1234"),
                                              QStringLiteral("--disable-webview")}));
    run = signInCompleteRun(StoreClient::Gog, binaries(), root(), QStringLiteral("c0de1234"), {});
    QCOMPARE(run.spec.arguments,
             QStringList({QStringLiteral("--auth-config-path"), gogAuthConfigPath(root()),
                          QStringLiteral("auth"), QStringLiteral("--code"),
                          QStringLiteral("c0de1234")}));
    QVERIFY(run.carriesSecrets);
    QVERIFY(accountStatusRun(StoreClient::Gog, binaries(), root()).carriesSecrets);
    QVERIFY(!signOutRun(StoreClient::Gog, binaries(), root()).has_value());
  }

  void accountStatus() {
    // Recorded, signed out.
    QVERIFY(!parseAccountStatus(StoreClient::Epic,
                                "{\"account\": \"<not logged in>\", \"games_available\": 0}")
                 .signedIn);
    QVERIFY(!parseAccountStatus(StoreClient::Gog, "null\n").signedIn);
    QVERIFY(!parseAccountStatus(StoreClient::Amazon,
                                "{\"Username\": \"<not logged in>\", \"LoggedIn\": false}")
                 .signedIn);
    // Signed in.
    const StoreAccountStatus epic =
        parseAccountStatus(StoreClient::Epic, "{\"account\": \"Jarrod\", \"games_available\": 12}");
    QVERIFY(epic.signedIn);
    QCOMPARE(epic.userName, QStringLiteral("Jarrod"));
    const StoreAccountStatus gog = parseAccountStatus(
        StoreClient::Gog, "{\"access_token\": \"tok-EN_abc123456\", \"user_id\": \"1\"}");
    QVERIFY(gog.signedIn);
    QCOMPARE(gog.accessToken, QStringLiteral("tok-EN_abc123456"));
    QVERIFY(parseAccountStatus(StoreClient::Amazon,
                               "log line\n{\"Username\": \"J\", \"LoggedIn\": true}\n")
                .signedIn);
  }

  void libraries() {
    const QByteArray epic = "[\n"
        "      {\"app_name\": \"Sugar\", \"app_title\": \"Rocket League\",\n"
        "       \"metadata\": {\"keyImages\": [{\"type\": \"DieselGameBox\", \"url\": \"https://cdn1.epicgames.com/a.jpg\"},\n"
        "                                  {\"type\": \"DieselGameBoxTall\", \"url\": \"https://cdn1.epicgames.com/tall.jpg\"}]}},\n"
        "      {\"app_name\": \"ea-title\", \"app_title\": \"Sold via EA\",\n"
        "       \"metadata\": {\"customAttributes\": {\"ThirdPartyManagedApp\": {\"value\": \"Origin\"}}}},\n"
        "      {\"app_name\": \"-bad\", \"app_title\": \"Bad id\"}\n"
        "    ]";
    const QVector<OwnedStoreGame> games = parseEpicLibrary(epic);
    QCOMPARE(games.size(), 1);
    QCOMPARE(games.first().id, QStringLiteral("Sugar"));
    QCOMPARE(games.first().imageUrl, QUrl(QStringLiteral("https://cdn1.epicgames.com/tall.jpg")));

    const QByteArray amazon = "[{\"id\": \"ent-1\", \"product\": {\"id\": \"amzn1.adg.product.1-2\",\n"
        "        \"title\": \"Some Game\", \"productDetail\": {\"details\": {\"logoUrl\": \"http://insecure/x.png\"},\n"
        "        \"iconUrl\": \"https://m.media-amazon.com/i.png\"}}}]";
    const QVector<OwnedStoreGame> owned = parseAmazonLibrary(amazon);
    QCOMPARE(owned.size(), 1);
    QCOMPARE(owned.first().id, QStringLiteral("amzn1.adg.product.1-2"));
    QCOMPARE(owned.first().imageUrl, QUrl(QStringLiteral("https://m.media-amazon.com/i.png")));

    const QByteArray gog = "{\"totalPages\": 3, \"products\": [\n"
        "        {\"id\": 1207658924, \"title\": \"Stardew\", \"image\": \"//images-1.gog-statics.com/abc\",\n"
        "         \"worksOn\": {\"Windows\": true, \"Mac\": true}, \"isGame\": true},\n"
        "        {\"id\": 5, \"title\": \"Mac only\", \"worksOn\": {\"Windows\": false}, \"isGame\": true},\n"
        "        {\"id\": 6, \"title\": \"A movie\", \"worksOn\": {\"Windows\": true}, \"isGame\": false}]}";
    int pages = 0;
    const QVector<OwnedStoreGame> gogGames = parseGogLibraryPage(gog, &pages);
    QCOMPARE(pages, 3);
    QCOMPARE(gogGames.size(), 1);
    QCOMPARE(gogGames.first().id, QStringLiteral("1207658924"));
    QCOMPARE(gogGames.first().imageUrl,
             QUrl(QStringLiteral("https://images-1.gog-statics.com/abc.jpg")));
    QCOMPARE(gogLibraryPageUrl(2).host(), QStringLiteral("embed.gog.com"));
  }

  void installArgv() {
    auto run = installRun(StoreClient::Epic, binaries(), root(), QStringLiteral("Sugar"),
                          QStringLiteral("/home/u/Games/Epic Games Store"));
    QVERIFY(run.has_value());
    QCOMPARE(run->spec.arguments,
             QStringList({QStringLiteral("-y"), QStringLiteral("install"), QStringLiteral("Sugar"),
                          QStringLiteral("--base-path"), QStringLiteral("/home/u/Games/Epic Games Store"),
                          QStringLiteral("--platform"), QStringLiteral("Windows"),
                          QStringLiteral("--skip-sdl")}));
    QVERIFY(!run->carriesSecrets);
    run = installRun(StoreClient::Gog, binaries(), root(), QStringLiteral("1207658924"),
                     QStringLiteral("/g"));
    QCOMPARE(run->spec.arguments,
             QStringList({QStringLiteral("--auth-config-path"), gogAuthConfigPath(root()),
                          QStringLiteral("download"), QStringLiteral("1207658924"),
                          QStringLiteral("--platform"), QStringLiteral("windows"),
                          QStringLiteral("--path"), QStringLiteral("/g"),
                          QStringLiteral("--skip-dlcs")}));
    run = installRun(StoreClient::Amazon, binaries(), root(), QStringLiteral("amzn1.x"),
                     QStringLiteral("/g"));
    QCOMPARE(run->spec.arguments, QStringList({QStringLiteral("install"), QStringLiteral("amzn1.x"),
                                               QStringLiteral("--base-path"), QStringLiteral("/g")}));
    QVERIFY(!installRun(StoreClient::Epic, binaries(), root(), QStringLiteral("--yes"),
                        QStringLiteral("/g")).has_value());
    QVERIFY(!installRun(StoreClient::Epic, binaries(), root(), QStringLiteral("App"),
                        QStringLiteral("relative")).has_value());
    QVERIFY(!installedInfoRun(StoreClient::Gog, binaries(), root(), QStringLiteral("1")).has_value());
  }

  void installedGames() {
    const QByteArray epic = "[{\"app_name\": \"Sugar\", \"install_path\": \"/g/rocketleague\",\n"
        "        \"executable\": \"Binaries\\\\Win64\\\\RocketLeague.exe\"},\n"
        "        {\"app_name\": \"Evil\", \"install_path\": \"/g/evil\", \"executable\": \"..\\\\..\\\\bin\\\\sh\"}]";
    const auto sugar = parseEpicInstalled(epic, QStringLiteral("Sugar"));
    QVERIFY(sugar.has_value());
    QCOMPARE(sugar->executable, QStringLiteral("/g/rocketleague/Binaries/Win64/RocketLeague.exe"));
    QVERIFY(!parseEpicInstalled(epic, QStringLiteral("Evil")).has_value());
    QVERIFY(!parseEpicInstalled(epic, QStringLiteral("Missing")).has_value());

    const QByteArray amazon = "[INFO] launching\n{\"command\": {\"instruction\": \"/g/Some Game/"
                              "bin/game.exe\", \"arguments\": []}, \"game_directory\": "
                              "\"/g/Some Game\", \"env\": {}}\n";
    const auto some = parseAmazonLaunchInfo(amazon);
    QVERIFY(some.has_value());
    QCOMPARE(some->executable, QStringLiteral("/g/Some Game/bin/game.exe"));
    QVERIFY(!parseAmazonLaunchInfo("{\"command\": {\"instruction\": \"/etc/passwd\"}, "
                                   "\"game_directory\": \"/g/x\"}").has_value());

    const QByteArray info = "{\"playTasks\": [\n"
        "        {\"type\": \"URLTask\", \"link\": \"https://example\"},\n"
        "        {\"type\": \"FileTask\", \"category\": \"tool\", \"path\": \"tools\\\\setup.exe\"},\n"
        "        {\"type\": \"FileTask\", \"isPrimary\": true, \"category\": \"game\", \"path\": \"bin\\\\Game.exe\"}]}";
    const auto gog = parseGogGameInfo(info, QStringLiteral("/g/Stardew"));
    QVERIFY(gog.has_value());
    QCOMPARE(gog->executable, QStringLiteral("/g/Stardew/bin/Game.exe"));
  }

  void gogInstallOnDiskIsCaseInsensitive() {
    QTemporaryDir base;
    QVERIFY(base.isValid());
    const QString folder = base.filePath(QStringLiteral("Stardew Valley"));
    QVERIFY(QDir().mkpath(folder + QStringLiteral("/Bin")));
    QFile exe(folder + QStringLiteral("/Bin/stardew.EXE"));
    QVERIFY(exe.open(QIODevice::WriteOnly));
    exe.close();
    QFile info(folder + QStringLiteral("/goggame-1453375253.info"));
    QVERIFY(info.open(QIODevice::WriteOnly));
    info.write("{\"playTasks\": [{\"type\": \"FileTask\", \"isPrimary\": true, \"path\": \"bin\\\\Stardew.exe\"}]}");
    info.close();
    const auto install = readGogInstall(base.path(), QStringLiteral("1453375253"));
    QVERIFY(install.has_value());
    QCOMPARE(install->installDir, folder);
    QCOMPARE(install->executable, folder + QStringLiteral("/Bin/stardew.EXE"));
    QVERIFY(!readGogInstall(base.path(), QStringLiteral("999")).has_value());
  }

  void progressLines() {
    QCOMPARE(parseDownloadProgress(QStringLiteral(
                 "[DLManager] INFO: = Progress: 12.34% (1/10), Running for 00:00:03, ETA: 00:00:30")),
             0.1234);
    QCOMPARE(parseDownloadProgress(QStringLiteral("[PROGRESS] INFO: = Progress: 99.00 990/1000, ")),
             0.99);
    QCOMPARE(parseDownloadProgress(QStringLiteral("= Progress: 100.00 5/5")), 1.0);
    QVERIFY(!parseDownloadProgress(QStringLiteral("Downloaded: 1.0 MiB")).has_value());
  }
};

QTEST_GUILESS_MAIN(tst_store_clients)
#include "tst_store_clients.moc"
