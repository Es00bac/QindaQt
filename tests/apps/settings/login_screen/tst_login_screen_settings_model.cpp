// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_login_screen/login_screen_settings_model.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QTemporaryDir>
#include <QTest>

using namespace QindaQt::Apps::SettingsLoginScreen;

namespace {

void writeFile(const QString &path, const QString &contents) {
  QFile file(path);
  QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Text),
           qPrintable(file.errorString()));
  file.write(contents.toUtf8());
}

// Fake writer: records payloads, applies them to the temp drop-in with the
// production merge/write function when told to succeed, and only completes
// when the test says so -- the asynchronous boundary the model must survive.
class FakeWriter final : public SddmConfigWriteClient {
  Q_OBJECT
public:
  explicit FakeWriter(QString targetFile) : m_targetFile(std::move(targetFile)) {}

  void write(const SddmOwnedChangeSet &changes) override {
    payloads.append(changes);
    m_inFlight = true;
  }
  [[nodiscard]] bool writeInFlight() const override { return m_inFlight; }

  void succeed() {
    QVERIFY(m_inFlight);
    QString error;
    QVERIFY2(writeOwnedChangeSetToFile(m_targetFile, payloads.constLast(), &error),
             qPrintable(error));
    m_inFlight = false;
    Q_EMIT writeFinished(true, QString());
  }
  void fail(const QString &errorText) {
    QVERIFY(m_inFlight);
    m_inFlight = false;
    Q_EMIT writeFinished(false, errorText);
  }

  QList<SddmOwnedChangeSet> payloads;

private:
  QString m_targetFile;
  bool m_inFlight = false;
};

class FakeProbe final : public LoginScreenAuthorityProbe {
  Q_OBJECT
public:
  void probe() override { Q_EMIT probeFinished(writable, reason); }
  bool writable = true;
  QString reason;
};

struct Fixture {
  QTemporaryDir root;
  QString etcD;
  QString ownedFile;
  // Raw aliases; the model owns the fakes through its own unique_ptrs.
  FakeWriter *writer = nullptr;
  FakeProbe *probe = nullptr;
  std::unique_ptr<LoginScreenSettingsModel> model;

  Fixture() {
    QVERIFY(root.isValid());
    // Themes: one QindaQt (marker), one third-party.
    const QString themes = root.filePath(QStringLiteral("themes"));
    QVERIFY(QDir().mkpath(themes + QStringLiteral("/qinda-a")));
    QVERIFY(QDir().mkpath(themes + QStringLiteral("/other-b")));
    writeFile(themes + QStringLiteral("/qinda-a/Main.qml"), QString());
    writeFile(themes + QStringLiteral("/qinda-a/metadata.desktop"),
              QStringLiteral("[SddmGreeterTheme]\nName=Qinda A\n"));
    writeFile(themes + QStringLiteral("/qinda-a/.qindaqt-sddm-theme"), QString());
    writeFile(themes + QStringLiteral("/other-b/Main.qml"), QString());
    writeFile(themes + QStringLiteral("/other-b/metadata.desktop"),
              QStringLiteral("[SddmGreeterTheme]\nName=Other B\n"));
    // Sessions: one wayland, one x11.
    const QString wayland = root.filePath(QStringLiteral("wayland-sessions"));
    const QString x11 = root.filePath(QStringLiteral("xsessions"));
    QVERIFY(QDir().mkpath(wayland));
    QVERIFY(QDir().mkpath(x11));
    writeFile(QDir(wayland).absoluteFilePath(QStringLiteral("qindaqt.desktop")),
              QStringLiteral("[Desktop Entry]\nName=QindaQt\nExec=qindaqt\n"));
    writeFile(QDir(x11).absoluteFilePath(QStringLiteral("plasmax11.desktop")),
              QStringLiteral("[Desktop Entry]\nName=Plasma (X11)\nExec=startplasma-x11\n"));
    // Users.
    const QString passwd = root.filePath(QStringLiteral("passwd"));
    writeFile(passwd, QStringLiteral(
        "root:x:0:0:root:/root:/bin/bash\n"
        "ada:x:1000:1000:Ada:/home/ada:/bin/bash\n"));
    // Config: gentoo pin + this route's drop-in path (created on write).
    etcD = root.filePath(QStringLiteral("sddm.conf.d"));
    QVERIFY(QDir().mkpath(etcD));
    writeFile(QDir(etcD).absoluteFilePath(QStringLiteral("01gentoo.conf")),
              QStringLiteral("[General]\nNumlock=none\n"));
    ownedFile = QDir(etcD).absoluteFilePath(QStringLiteral("zzz-qindaqt-settings.conf"));

    LoginScreenPaths paths;
    paths.themesDirectory = themes;
    paths.waylandSessionDirectories = {wayland};
    paths.xSessionDirectories = {x11};
    paths.passwdFile = passwd;
    paths.sddmConfigScanDirectories = {etcD};
    paths.sddmLegacyMainFile = QString();
    paths.ownedConfigFile = ownedFile;

    auto writerOwner = std::make_unique<FakeWriter>(ownedFile);
    writer = writerOwner.get();
    auto probeOwner = std::make_unique<FakeProbe>();
    probe = probeOwner.get();
    model = std::make_unique<LoginScreenSettingsModel>(
        paths, std::move(writerOwner), std::move(probeOwner),
        QStringLiteral("ada"));
  }
};

} // namespace

class LoginScreenSettingsModelTest final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void initialProjectionReflectsMergedTruth();
  void selectThemeWritesAndRefreshes();
  void selectThemeRejectsAnUninstalledTheme();
  void deniedProbeMakesTheModelReadOnly();
  void failedWriteKeepsThePreviousTruth();
  void queuedWritesAreSerialized();
  void autologinEnableRequiresASessionThenWritesUserAndSession();
  void autologinDisableWritesAnEmptyUser();
  void shadowedKeysSurfaceAnOverrideMessage();
};

void LoginScreenSettingsModelTest::initialProjectionReflectsMergedTruth() {
  Fixture f;
  const QList<SddmThemeEntry> themes = f.model->themeEntries();
  QCOMPARE(themes.size(), 2);
  QCOMPARE(themes.at(0).id, QStringLiteral("qinda-a")); // QindaQt first
  QCOMPARE(themes.at(0).name, QStringLiteral("Qinda A"));
  QCOMPARE(f.model->sessionEntries().size(), 2);
  QCOMPARE(f.model->loginUsers(), QStringList({QStringLiteral("ada")}));
  QCOMPARE(f.model->numlockMode(), QStringLiteral("none"));
  QCOMPARE(f.model->currentTheme(), QString());
  QVERIFY(f.model->writable());
  QVERIFY(!f.model->busy());
}

void LoginScreenSettingsModelTest::selectThemeWritesAndRefreshes() {
  Fixture f;
  QVERIFY(f.model->selectTheme(QStringLiteral("qinda-a")));
  QVERIFY(f.model->busy());
  auto *writer = f.writer;
  QCOMPARE(writer->payloads.size(), 1);
  QCOMPARE(*writer->payloads.constLast().theme, QStringLiteral("qinda-a"));
  writer->succeed();
  QVERIFY(!f.model->busy());
  QCOMPARE(f.model->currentTheme(), QStringLiteral("qinda-a"));
  QFile file(f.ownedFile);
  QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
  QVERIFY(QString::fromUtf8(file.readAll())
              .contains(QStringLiteral("Current=qinda-a")));
}

void LoginScreenSettingsModelTest::selectThemeRejectsAnUninstalledTheme() {
  Fixture f;
  QVERIFY(!f.model->selectTheme(QStringLiteral("not-installed")));
  QVERIFY(f.model->errorText().contains(QStringLiteral("not-installed")));
  auto *writer = f.writer;
  QCOMPARE(writer->payloads.size(), 0);
}

void LoginScreenSettingsModelTest::deniedProbeMakesTheModelReadOnly() {
  QTemporaryDir root;
  QVERIFY(root.isValid());
  // Point every path at something absent; the point is the probe's answer.
  LoginScreenPaths paths;
  paths.themesDirectory = root.filePath(QStringLiteral("absent-themes"));
  paths.passwdFile = root.filePath(QStringLiteral("absent-passwd"));
  auto probe = std::make_unique<FakeProbe>();
  probe->writable = false;
  probe->reason = QStringLiteral("not allowed");
  auto writer = std::make_unique<FakeWriter>(root.filePath(QStringLiteral("x")));
  LoginScreenSettingsModel model(paths, std::move(writer), std::move(probe),
                                 QStringLiteral("ada"));
  QVERIFY(!model.writable());
  QCOMPARE(model.readOnlyReason(), QStringLiteral("not allowed"));
  QVERIFY(!model.selectTheme(QStringLiteral("anything")));
  QCOMPARE(model.errorText(), QStringLiteral("not allowed"));
}

void LoginScreenSettingsModelTest::failedWriteKeepsThePreviousTruth() {
  Fixture f;
  QVERIFY(f.model->selectTheme(QStringLiteral("qinda-a")));
  auto *writer = f.writer;
  writer->fail(QStringLiteral("Authentication was refused or cancelled."));
  QVERIFY(!f.model->busy());
  QCOMPARE(f.model->errorText(),
           QStringLiteral("Authentication was refused or cancelled."));
  // The on-disk truth never moved, so neither did the model.
  QCOMPARE(f.model->currentTheme(), QString());
}

void LoginScreenSettingsModelTest::queuedWritesAreSerialized() {
  Fixture f;
  QVERIFY(f.model->selectTheme(QStringLiteral("qinda-a")));
  QVERIFY(f.model->setNumlockMode(QStringLiteral("on")));
  auto *writer = f.writer;
  // One write in flight, one queued; the writer never sees two at once.
  QCOMPARE(writer->payloads.size(), 1);
  QVERIFY(f.model->busy());
  writer->succeed();
  QCOMPARE(writer->payloads.size(), 2);
  QCOMPARE(*writer->payloads.constLast().numlock, QStringLiteral("on"));
  writer->succeed();
  QVERIFY(!f.model->busy());
  QCOMPARE(f.model->currentTheme(), QStringLiteral("qinda-a"));
  QCOMPARE(f.model->numlockMode(), QStringLiteral("on"));
}

void LoginScreenSettingsModelTest::
    autologinEnableRequiresASessionThenWritesUserAndSession() {
  Fixture f;
  // No default session chosen anywhere: enabling must refuse with guidance.
  QVERIFY(!f.model->setAutologinEnabled(true));
  QVERIFY(f.model->errorText().contains(QStringLiteral("default session")));

  f.model->clearError();
  QVERIFY(f.model->selectDefaultSession(QStringLiteral("qindaqt.desktop")));
  auto *writer = f.writer;
  writer->succeed();
  QVERIFY(f.model->setAutologinEnabled(true));
  writer->succeed();
  const SddmOwnedChangeSet &payload = writer->payloads.constLast();
  QVERIFY(payload.autologinUser.has_value());
  QCOMPARE(*payload.autologinUser, QStringLiteral("ada")); // current user
  QCOMPARE(*payload.autologinSession, QStringLiteral("qindaqt.desktop"));
  QVERIFY(f.model->autologinEnabled());
  QCOMPARE(f.model->autologinUser(), QStringLiteral("ada"));
}

void LoginScreenSettingsModelTest::autologinDisableWritesAnEmptyUser() {
  Fixture f;
  // Arrange an enabled state on disk first.
  writeFile(f.ownedFile, QStringLiteral(
      "[Autologin]\nUser=ada\nSession=qindaqt.desktop\n"));
  f.model->refresh();
  QVERIFY(f.model->autologinEnabled());

  QVERIFY(f.model->setAutologinEnabled(false));
  auto *writer = f.writer;
  writer->succeed();
  const SddmOwnedChangeSet &payload = writer->payloads.constLast();
  QVERIFY(payload.autologinUser.has_value());
  QCOMPARE(*payload.autologinUser, QString());
  QVERIFY(!f.model->autologinEnabled());
  // The pinned session survives the toggle, inert.
  QCOMPARE(f.model->defaultSession(), QStringLiteral("qindaqt.desktop"));
}

void LoginScreenSettingsModelTest::shadowedKeysSurfaceAnOverrideMessage() {
  Fixture f;
  writeFile(f.ownedFile, QStringLiteral("[Theme]\nCurrent=qinda-a\n"));
  writeFile(QDir(f.etcD).absoluteFilePath(QStringLiteral("zzzz-operator.conf")),
            QStringLiteral("[Theme]\nCurrent=operator\n"));
  f.model->refresh();
  QVERIFY(f.model->overrideText().contains(QStringLiteral("zzzz-operator.conf")));
  QVERIFY(f.model->overrideText().contains(QStringLiteral("theme")));
  QCOMPARE(f.model->currentTheme(), QStringLiteral("operator"));
}

QTEST_MAIN(LoginScreenSettingsModelTest)
#include "tst_login_screen_settings_model.moc"
