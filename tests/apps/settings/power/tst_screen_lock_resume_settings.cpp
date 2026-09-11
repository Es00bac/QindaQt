// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/apps/settings_power/screen_lock_settings.h>

#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtCore/QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace QindaQt::Apps::SettingsPower;

namespace {
class FakeConfigureClient final : public ScreenLockConfigureClient {
  Q_OBJECT
public:
  using ScreenLockConfigureClient::ScreenLockConfigureClient;
  void requestConfigure() override { ++requests; }
  int requests = 0;
  void finish(bool accepted, const QString &error = {}) { Q_EMIT configured(accepted, error); }
};

QString fileBytes(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QString::fromUtf8(file.readAll());
}

void writeRaw(const QString &path, const QByteArray &contents) {
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QVERIFY(file.write(contents) == contents.size());
}
} // namespace

class ScreenLockResumeSettingsTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void loadDefaultsAndRoundTripNewKeys();
  void invalidGraceValuesAreRejectedWithoutSaving();
  void unrelatedKeysAndGroupsSurviveResumeAndGraceSaves();
  void unchangedValuesAreNeverWritten();
  void oneMutationRequestsConfigureExactlyOnce();
  void configureFailureSurfacesForBothNewKeys();
  void graceRetryAfterSaveFailureKeepsExternalEdits();
};

void ScreenLockResumeSettingsTest::loadDefaultsAndRoundTripNewKeys() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));

  // Absent keys mean the locker's own defaults, not invented values.
  writeRaw(path, "[Daemon]\nAutolock=false\n");
  IniScreenLockPreferencesStore store(path);
  ScreenLockPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QVERIFY(preferences.lockOnResume);
  QCOMPARE(preferences.lockGraceSeconds, 5);

  writeRaw(path,
           "[Daemon]\nAutolock=true\nTimeout=17\nLockOnResume=false\nLockGrace=30\n");
  QVERIFY(store.load(&preferences, &error));
  QVERIFY(!preferences.lockOnResume);
  QCOMPARE(preferences.lockGraceSeconds, 30);

  preferences.lockOnResume = true;
  preferences.lockGraceSeconds = 300;
  QVERIFY(store.save(preferences, &error));
  ScreenLockPreferences reloaded;
  QVERIFY(store.load(&reloaded, &error));
  QVERIFY(reloaded.lockOnResume);
  QCOMPARE(reloaded.lockGraceSeconds, 300);
  QCOMPARE(reloaded.automaticLock, true);
  QCOMPARE(reloaded.timeoutMinutes, 17.0);
  const QString after = fileBytes(path);
  QVERIFY(after.contains(QStringLiteral("LockOnResume=true")));
  QVERIFY(after.contains(QStringLiteral("LockGrace=300")));
}

void ScreenLockResumeSettingsTest::invalidGraceValuesAreRejectedWithoutSaving() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  writeRaw(path, "[Daemon]\nLockOnResume=false\nLockGrace=30\n");
  FakeConfigureClient configure;
  ScreenLockSettingsModel model(
      std::make_unique<IniScreenLockPreferencesStore>(path), configure);

  for (const int invalid : {1, 4, -1, 601, 900}) {
    QVERIFY(!ScreenLockSettingsModel::isValidGraceSeconds(invalid));
    QVERIFY(!model.setLockGraceSeconds(invalid));
    QVERIFY(!model.errorText().isEmpty());
    QCOMPARE(configure.requests, 0);
  }
  for (const int valid : ScreenLockSettingsModel::GraceChoices) {
    QVERIFY(ScreenLockSettingsModel::isValidGraceSeconds(valid));
  }
  // The rejected mutation left both the model and the file untouched.
  QVERIFY(!model.lockOnResume());
  QCOMPARE(model.lockGraceSeconds(), 30);
  QCOMPARE(fileBytes(path),
           QString::fromUtf8("[Daemon]\nLockOnResume=false\nLockGrace=30\n"));
}

void ScreenLockResumeSettingsTest::unrelatedKeysAndGroupsSurviveResumeAndGraceSaves() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  writeRaw(path,
           "[Daemon]\nAutolock=true\nTimeout=17\nRequirePassword=true\nLockGrace=900\n"
           "[Greeter]\nTheme=org.qindaqt.lock\n");

  FakeConfigureClient configure;
  ScreenLockSettingsModel model(
      std::make_unique<IniScreenLockPreferencesStore>(path), configure);
  QCOMPARE(model.lockGraceSeconds(), 900);

  QVERIFY(model.setLockOnResume(true));
  configure.finish(true);
  // The file has no explicit LockOnResume key, so it already means the
  // default (true): this mutation must write nothing at all.
  QVERIFY(!fileBytes(path).contains(QStringLiteral("LockOnResume")));
  // A QSettings write normalizes the whole file, so "unrelated keys preserved"
  // means their values survive, not the original bytes.
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    QCOMPARE(settings.value(QStringLiteral("LockOnResume"), true).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("LockGrace")).toInt(), 900);
    QCOMPARE(settings.value(QStringLiteral("RequirePassword")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("Timeout")).toInt(), 17);
    settings.endGroup();
    settings.beginGroup(QStringLiteral("Greeter"));
    QCOMPARE(settings.value(QStringLiteral("Theme")).toString(),
             QStringLiteral("org.qindaqt.lock"));
  }

  QVERIFY(model.setLockGraceSeconds(60));
  configure.finish(true);
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    QCOMPARE(settings.value(QStringLiteral("LockGrace")).toInt(), 60);
    QCOMPARE(settings.value(QStringLiteral("RequirePassword")).toBool(), true);
    QCOMPARE(settings.value(QStringLiteral("Timeout")).toInt(), 17);
    settings.endGroup();
    settings.beginGroup(QStringLiteral("Greeter"));
    QCOMPARE(settings.value(QStringLiteral("Theme")).toString(),
             QStringLiteral("org.qindaqt.lock"));
  }
}

void ScreenLockResumeSettingsTest::unchangedValuesAreNeverWritten() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  // The padding bytes (comment and spacing) only survive when QSettings never
  // rewrites the file; any setValue dirties the config and normalizes it away.
  const QByteArray pristine =
      "; locker config\n[Daemon]\nAutolock = true\nTimeout = 17\nLockOnResume = true\n"
      "LockGrace = 30\n";
  writeRaw(path, pristine);

  IniScreenLockPreferencesStore store(path);
  ScreenLockPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QVERIFY(store.save(preferences, &error));
  QCOMPARE(fileBytes(path), QString::fromUtf8(pristine));

  // A single changed key is written while the other three keep their values.
  // Any write rewrites and normalizes the whole file, so only the no-write
  // case above can assert byte identity.
  const ScreenLockPreferences before = preferences;
  preferences.lockGraceSeconds = 5;
  QVERIFY(store.save(preferences, &error));
  const QString after = fileBytes(path);
  QVERIFY(after.contains(QStringLiteral("LockGrace=5")));
  QVERIFY(after.contains(QStringLiteral("Autolock=true")));
  QVERIFY(after.contains(QStringLiteral("Timeout=17")));
  QVERIFY(after.contains(QStringLiteral("LockOnResume=true")));
  preferences = before;
}

void ScreenLockResumeSettingsTest::oneMutationRequestsConfigureExactlyOnce() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  writeRaw(path, "[Daemon]\nAutolock=true\nLockOnResume=false\nLockGrace=300\n");
  FakeConfigureClient configure;
  ScreenLockSettingsModel model(
      std::make_unique<IniScreenLockPreferencesStore>(path), configure);

  QVERIFY(model.setLockOnResume(true));
  QCOMPARE(configure.requests, 1);
  configure.finish(true);
  QVERIFY(model.lockOnResume());
  QVERIFY(fileBytes(path).contains(QStringLiteral("LockOnResume=true")));

  QVERIFY(model.setLockGraceSeconds(0));
  QCOMPARE(configure.requests, 2);
  configure.finish(true);
  QCOMPARE(model.lockGraceSeconds(), 0);

  // Retry after a completed mutation re-runs the live step only.
  QVERIFY(model.retryLiveApply());
  QCOMPARE(configure.requests, 3);
  configure.finish(true);
}

void ScreenLockResumeSettingsTest::configureFailureSurfacesForBothNewKeys() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  writeRaw(path, "[Daemon]\nAutolock=true\nLockOnResume=false\nLockGrace=300\n");
  FakeConfigureClient configure;
  ScreenLockSettingsModel model(
      std::make_unique<IniScreenLockPreferencesStore>(path), configure);

  QVERIFY(model.setLockOnResume(true));
  configure.finish(false, QStringLiteral("locker refused"));
  QVERIFY(model.lockOnResume());
  QVERIFY(model.errorText().contains(QStringLiteral("saved")));
  QVERIFY(model.errorText().contains(QStringLiteral("locker refused")));

  QVERIFY(model.setLockGraceSeconds(30));
  configure.finish(false, QStringLiteral("gone"));
  QCOMPARE(model.lockGraceSeconds(), 30);
  QVERIFY(model.errorText().contains(QStringLiteral("gone")));
  QVERIFY(model.retryLiveApply());
  QCOMPARE(configure.requests, 3);
  configure.finish(true);
  QVERIFY(model.errorText().isEmpty());
}

void ScreenLockResumeSettingsTest::graceRetryAfterSaveFailureKeepsExternalEdits() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  writeRaw(path, "[Daemon]\nAutolock=true\nLockOnResume=false\nLockGrace=300\n");
  FakeConfigureClient configure;

  class FailOnceStore final : public ScreenLockPreferencesStore {
  public:
    explicit FailOnceStore(const QString &path) : m_store(path) {}
    bool load(ScreenLockPreferences *preferences, QString *error) override {
      return m_store.load(preferences, error);
    }
    bool save(const ScreenLockPreferences &preferences, QString *error) override {
      if (m_fail) {
        m_fail = false;
        if (error) *error = QStringLiteral("write failed");
        return false;
      }
      return m_store.save(preferences, error);
    }
    bool m_fail = true;

  private:
    IniScreenLockPreferencesStore m_store;
  };

  ScreenLockSettingsModel model(std::make_unique<FailOnceStore>(path), configure);
  QVERIFY(!model.setLockGraceSeconds(5));
  QCOMPARE(configure.requests, 0);

  // External writer touches the resume key while the grace save is failing.
  writeRaw(path, "[Daemon]\nAutolock=true\nLockOnResume=true\nLockGrace=300\n");
  QVERIFY(model.retryLiveApply());
  QCOMPARE(configure.requests, 1);
  configure.finish(true);

  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    QCOMPARE(settings.value(QStringLiteral("LockGrace")).toInt(), 5);
    QCOMPARE(settings.value(QStringLiteral("LockOnResume")).toBool(), true);
  }
}

QTEST_GUILESS_MAIN(ScreenLockResumeSettingsTest)
#include "tst_screen_lock_resume_settings.moc"
