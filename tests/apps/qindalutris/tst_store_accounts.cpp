// SPDX-License-Identifier: GPL-3.0-or-later
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "job_fakes.h"
#include "library_controller.h"
#include "store_accounts.h"

using namespace QindaQt::QindaLutris;
using QindaQt::QindaLutris::TestSupport::FakeRunner;

// ADR-0275 section 8: the Accounts controller over scripted client runs.
// legendary / gogdl / nile are empty executables on a private PATH (so the
// tool discovery is real) and every run goes to a FakeRunner.
class tst_store_accounts : public QObject {
  Q_OBJECT

  QTemporaryDir m_dir;
  QList<FakeRunner *> m_runners; // Epic, GOG, Amazon (construction order)

  std::unique_ptr<LibraryController> makeLibrary() {
    auto library = std::make_unique<LibraryController>();
    library->setConfigRoot(m_dir.filePath(QStringLiteral("config")));
    library->setSteamCandidates({});
    library->setDesktopDataRoots({});
    library->setLutrisDatabasePath(m_dir.filePath(QStringLiteral("none.db")));
    library->refresh();
    return library;
  }

  StoreAccounts::RunnerFactory factory() {
    m_runners.clear();
    return [this](QObject *parent) {
      auto *runner = new FakeRunner;
      runner->setParent(parent);
      m_runners.append(runner);
      return runner;
    };
  }

  static ProcessRunResult ok(const QByteArray &out) {
    ProcessRunResult result;
    result.started = true;
    result.exitCode = 0;
    result.standardOutput = out;
    return result;
  }

  static QVariantMap row(const StoreAccounts &accounts, const QString &id) {
    for (const QVariant &value : accounts.accounts()) {
      if (value.toMap().value(QStringLiteral("id")).toString() == id) {
        return value.toMap();
      }
    }
    return {};
  }

  static bool idle(const StoreAccounts &accounts, const QString &id) {
    return !row(accounts, id).value(QStringLiteral("busy")).toBool();
  }

private Q_SLOTS:
  void initTestCase() {
    QVERIFY(m_dir.isValid());
    const QString bin = m_dir.filePath(QStringLiteral("bin"));
    QVERIFY(QDir().mkpath(bin));
    for (const char *name : {"legendary", "gogdl", "nile"}) {
      QFile tool(QDir(bin).filePath(QString::fromLatin1(name)));
      QVERIFY(tool.open(QIODevice::WriteOnly));
      tool.write("#!/bin/sh\nexit 1\n");
      tool.close();
      tool.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }
    qputenv("PATH", bin.toLocal8Bit());
  }

  void epicRefreshListsOwnedGames() {
    auto library = makeLibrary();
    StoreAccounts accounts(library.get(), false, factory());
    QCOMPARE(m_runners.size(), 3);
    m_runners[0]->respond = [](const ProcessRunSpec &spec) {
      if (spec.arguments.first() == QLatin1String("status")) {
        return ok("{\"account\": \"Jarrod\"}");
      }
      return ok("[{\"app_name\": \"Sugar\", \"app_title\": \"Rocket League\"}]");
    };
    m_runners[1]->respond = [](const ProcessRunSpec &) { return ok("null"); };
    m_runners[2]->respond = [](const ProcessRunSpec &) {
      return ok("{\"Username\": \"<not logged in>\", \"LoggedIn\": false}");
    };
    accounts.refreshAll();
    QTRY_VERIFY(idle(accounts, QStringLiteral("egs")) && idle(accounts, QStringLiteral("gog")) &&
                idle(accounts, QStringLiteral("amazon")));
    const QVariantMap epic = row(accounts, QStringLiteral("egs"));
    QVERIFY(epic.value(QStringLiteral("available")).toBool());
    QVERIFY(epic.value(QStringLiteral("signedIn")).toBool());
    QCOMPARE(epic.value(QStringLiteral("userName")).toString(), QStringLiteral("Jarrod"));
    QCOMPARE(epic.value(QStringLiteral("message")).toString(),
             QStringLiteral("1 game in your library."));
    const QVariantList games = accounts.ownedGames(QStringLiteral("egs"));
    QCOMPARE(games.size(), 1);
    QCOMPARE(games.first().toMap().value(QStringLiteral("installedTitleId")).toString(), QString());
    QVERIFY(!row(accounts, QStringLiteral("gog")).value(QStringLiteral("signedIn")).toBool());
    QCOMPARE(row(accounts, QStringLiteral("amazon")).value(QStringLiteral("message")).toString(),
             QStringLiteral("Not signed in."));
    // Every client read QindaLutris's own configuration.
    QCOMPARE(m_runners[0]->specs.first().environment.value(QStringLiteral("LEGENDARY_CONFIG_PATH")),
             m_dir.filePath(QStringLiteral("config/stores/legendary")));
  }

  void gogSignInExchangesTheCodeAndNeverShowsIt() {
    auto library = makeLibrary();
    StoreAccounts accounts(library.get(), false, factory());
    FakeRunner *gog = m_runners[1];
    gog->respond = [](const ProcessRunSpec &spec) {
      return spec.arguments.contains(QStringLiteral("--code"))
                 ? ok("{\"access_token\": \"tok-abcdefgh123\", \"user_id\": \"1\"}")
                 : ok("null");
    };
    accounts.finishSignIn(
        QStringLiteral("gog"),
        QStringLiteral("https://embed.gog.com/on_login_success?origin=client&code=SeCrEt-code-123"));
    QTRY_VERIFY(idle(accounts, QStringLiteral("gog")));
    QVERIFY(gog->specs.size() >= 2);
    QVERIFY(gog->specs.first().arguments.contains(QStringLiteral("SeCrEt-code-123")));
    for (const QVariant &value : accounts.accounts()) {
      QVERIFY(!value.toMap().value(QStringLiteral("message")).toString().contains(
          QStringLiteral("SeCrEt")));
    }
  }

  void gogRefusedCodeIsNotASignIn() {
    auto library = makeLibrary();
    StoreAccounts accounts(library.get(), false, factory());
    m_runners[1]->respond = [](const ProcessRunSpec &) { return ok("{\"error\": true}"); };
    accounts.finishSignIn(QStringLiteral("gog"), QStringLiteral("abcdefgh123"));
    QTRY_VERIFY(idle(accounts, QStringLiteral("gog")));
    QCOMPARE(m_runners[1]->specs.size(), 1); // no status/library after a refusal
    QVERIFY(row(accounts, QStringLiteral("gog")).value(QStringLiteral("message")).toString()
                .startsWith(QStringLiteral("Sign-in did not work")));
  }

  void unusablePasteIsOneSentence() {
    auto library = makeLibrary();
    StoreAccounts accounts(library.get(), false, factory());
    accounts.finishSignIn(QStringLiteral("egs"), QStringLiteral("I clicked sign in"));
    QVERIFY(m_runners[0]->specs.isEmpty());
    QVERIFY(row(accounts, QStringLiteral("egs")).value(QStringLiteral("message")).toString()
                .contains(QStringLiteral("did not look like a sign-in code")));
  }

  void amazonSignInStartsWithNile() {
    auto library = makeLibrary();
    StoreAccounts accounts(library.get(), false, factory());
    m_runners[2]->respond = [](const ProcessRunSpec &) {
      return ok("{\"client_id\": \"" + QByteArray(94, 'a') + "\", \"code_verifier\": \""
                + QByteArray(43, 'b') + "\", \"serial\": \"" + QByteArray(32, 'c')
                + "\", \"url\": \"https://amazon.com/ap/signin?x=1\"}");
    };
    QSignalSpy ready(&accounts, &StoreAccounts::signInReady);
    accounts.beginSignIn(QStringLiteral("amazon"));
    QTRY_COMPARE(ready.size(), 1);
    QCOMPARE(ready.first().at(1).toUrl().host(), QStringLiteral("amazon.com"));
    accounts.beginSignIn(QStringLiteral("egs"));
    QCOMPARE(ready.size(), 2);
    QCOMPARE(ready.last().at(1).toUrl(), epicSignInUrl());
  }

  void gogSignOutRemovesTheSignIn() {
    auto library = makeLibrary();
    const QString file = gogAuthConfigPath(storesRootFor(library->configRoot()));
    QVERIFY(QDir().mkpath(QFileInfo(file).absolutePath()));
    QFile auth(file);
    QVERIFY(auth.open(QIODevice::WriteOnly));
    auth.write("{}");
    auth.close();
    StoreAccounts accounts(library.get(), false, factory());
    accounts.signOut(QStringLiteral("gog"));
    QVERIFY(!QFile::exists(file));
    QCOMPARE(row(accounts, QStringLiteral("gog")).value(QStringLiteral("message")).toString(),
             QStringLiteral("Signed out."));
  }
};

QTEST_GUILESS_MAIN(tst_store_accounts)
#include "tst_store_accounts.moc"
