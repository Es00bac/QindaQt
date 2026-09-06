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

class FakeStore final : public ScreenLockPreferencesStore {
public:
  bool load(ScreenLockPreferences *preferences, QString *error) override {
    if (!loadOk) { if (error) *error = QStringLiteral("read failed"); return false; }
    *preferences = stored; return true;
  }
  bool save(const ScreenLockPreferences &preferences, QString *error) override {
    if (!saveOk) { if (error) *error = QStringLiteral("write failed"); return false; }
    stored = preferences; ++saves; return true;
  }
  ScreenLockPreferences stored{false, 13};
  bool loadOk = true;
  bool saveOk = true;
  int saves = 0;
};

class FailingOnceIniStore final : public ScreenLockPreferencesStore {
public:
  explicit FailingOnceIniStore(const QString &path) : m_store(path) {}

  bool load(ScreenLockPreferences *preferences, QString *error) override {
    return m_store.load(preferences, error);
  }
  bool save(const ScreenLockPreferences &preferences, QString *error) override {
    if (m_failFirstSave) {
      m_failFirstSave = false;
      if (error) *error = QStringLiteral("write failed");
      return false;
    }
    return m_store.save(preferences, error);
  }

private:
  IniScreenLockPreferencesStore m_store;
  bool m_failFirstSave = true;
};
} // namespace

class ScreenLockSettingsTest final : public QObject {
  Q_OBJECT
private Q_SLOTS:
  void iniStorePreservesOtherLockerPreferences();
  void automaticLockAndTimeoutPersistThenRequestLiveReload();
  void liveReloadFailureStaysVisibleWithoutRollingBackPreference();
  void retryAfterLoadFailureReloadsStorageInsteadOfConfiguring();
  void retryAfterSaveFailureSavesBeforeConfiguring();
  void externalEditsToUntouchedKeysSurviveBothDirections();
  void saveRetryMergesExternalUntouchedKey();
  void hugeFiniteTimeoutIsClampedBeforeConversion();
};

void ScreenLockSettingsTest::iniStorePreservesOtherLockerPreferences() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("[Daemon]\nAutolock=false\nTimeout=17\nRequirePassword=true\nLockOnResume=true\n");
  file.close();

  IniScreenLockPreferencesStore store(path);
  ScreenLockPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QVERIFY(!preferences.automaticLock);
  QCOMPARE(preferences.timeoutMinutes, 17.0);
  preferences.timeoutMinutes = 22;
  QVERIFY(store.save(preferences, &error));
  QVERIFY(file.open(QIODevice::ReadOnly));
  const QString contents = QString::fromUtf8(file.readAll());
  QVERIFY(contents.contains(QStringLiteral("RequirePassword=true")));
  QVERIFY(contents.contains(QStringLiteral("LockOnResume=true")));
  QVERIFY(contents.contains(QStringLiteral("Autolock=false")));
  QVERIFY(contents.contains(QStringLiteral("Timeout=22")));
}

void ScreenLockSettingsTest::automaticLockAndTimeoutPersistThenRequestLiveReload() {
  auto store = std::make_unique<FakeStore>();
  FakeStore *rawStore = store.get();
  FakeConfigureClient configure;
  ScreenLockSettingsModel model(std::move(store), configure);
  QVERIFY(!model.automaticLock());
  QCOMPARE(model.timeoutMinutes(), 13);

  QVERIFY(model.setAutomaticLock(true));
  QVERIFY(model.busy());
  QCOMPARE(configure.requests, 1);
  QVERIFY(rawStore->stored.automaticLock);
  configure.finish(true);
  QVERIFY(!model.busy());
  QVERIFY(model.errorText().isEmpty());

  QVERIFY(model.setTimeoutMinutes(20));
  QCOMPARE(rawStore->stored.timeoutMinutes, 20.0);
  QCOMPARE(configure.requests, 2);
  configure.finish(true);
  QVERIFY(!model.setTimeoutMinutes(0));
  QCOMPARE(rawStore->saves, 2);
}

void ScreenLockSettingsTest::liveReloadFailureStaysVisibleWithoutRollingBackPreference() {
  auto store = std::make_unique<FakeStore>();
  FakeStore *rawStore = store.get();
  FakeConfigureClient configure;
  ScreenLockSettingsModel model(std::move(store), configure);
  QVERIFY(model.setAutomaticLock(true));
  configure.finish(false, QStringLiteral("service unavailable"));
  QVERIFY(model.automaticLock());
  QVERIFY(rawStore->stored.automaticLock);
  QVERIFY(model.errorText().contains(QStringLiteral("saved")));
  QVERIFY(model.errorText().contains(QStringLiteral("service unavailable")));
  QVERIFY(model.retryLiveApply());
  QCOMPARE(configure.requests, 2);
}

void ScreenLockSettingsTest::retryAfterLoadFailureReloadsStorageInsteadOfConfiguring() {
  auto store = std::make_unique<FakeStore>();
  FakeStore *rawStore = store.get();
  rawStore->loadOk = false;
  FakeConfigureClient configure;
  ScreenLockSettingsModel model(std::move(store), configure);
  QVERIFY(!model.errorText().isEmpty());

  QVERIFY(!model.retryLiveApply());
  QCOMPARE(configure.requests, 0);
  QVERIFY(!model.errorText().isEmpty());

  rawStore->loadOk = true;
  QVERIFY(model.retryLiveApply());
  QCOMPARE(configure.requests, 0);
  QVERIFY(model.errorText().isEmpty());
  QVERIFY(!model.automaticLock());
  QCOMPARE(model.timeoutMinutes(), 13);
}

void ScreenLockSettingsTest::retryAfterSaveFailureSavesBeforeConfiguring() {
  auto store = std::make_unique<FakeStore>();
  FakeStore *rawStore = store.get();
  FakeConfigureClient configure;
  ScreenLockSettingsModel model(std::move(store), configure);

  rawStore->saveOk = false;
  QVERIFY(!model.setAutomaticLock(true));
  QVERIFY(!model.automaticLock());
  QVERIFY(!rawStore->stored.automaticLock);
  QVERIFY(!model.errorText().isEmpty());
  QCOMPARE(configure.requests, 0);

  rawStore->saveOk = true;
  QVERIFY(model.retryLiveApply());
  QVERIFY(rawStore->stored.automaticLock);
  QCOMPARE(configure.requests, 1);
  configure.finish(true);
  QVERIFY(model.automaticLock());
  QVERIFY(model.errorText().isEmpty());
}

void ScreenLockSettingsTest::externalEditsToUntouchedKeysSurviveBothDirections() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    settings.setValue(QStringLiteral("Autolock"), false);
    settings.setValue(QStringLiteral("Timeout"), 17);
  }

  FakeConfigureClient configure;
  ScreenLockSettingsModel model(
      std::make_unique<IniScreenLockPreferencesStore>(path), configure);

  // External agent edits Timeout while the page is open; toggling Autolock
  // must keep 42.
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    settings.setValue(QStringLiteral("Timeout"), 42);
  }
  QVERIFY(model.setAutomaticLock(true));
  configure.finish(true);
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    QVERIFY(settings.value(QStringLiteral("Autolock")).toBool());
    QCOMPARE(settings.value(QStringLiteral("Timeout")).toInt(), 42);
  }

  // Reverse direction: external Autolock edit survives a timeout save.
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    settings.setValue(QStringLiteral("Autolock"), false);
  }
  QVERIFY(model.setTimeoutMinutes(30));
  configure.finish(true);
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    QVERIFY(!settings.value(QStringLiteral("Autolock")).toBool());
    QCOMPARE(settings.value(QStringLiteral("Timeout")).toInt(), 30);
  }
}

void ScreenLockSettingsTest::saveRetryMergesExternalUntouchedKey() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    settings.setValue(QStringLiteral("Autolock"), false);
    settings.setValue(QStringLiteral("Timeout"), 17);
  }

  FakeConfigureClient configure;
  ScreenLockSettingsModel model(std::make_unique<FailingOnceIniStore>(path), configure);
  QVERIFY(!model.setAutomaticLock(true));

  // This writer changes the untouched key after the first save failed.
  {
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Daemon"));
    settings.setValue(QStringLiteral("Timeout"), 42);
  }
  QVERIFY(model.retryLiveApply());
  QCOMPARE(configure.requests, 1);
  configure.finish(true);

  QSettings settings(path, QSettings::IniFormat);
  settings.beginGroup(QStringLiteral("Daemon"));
  QVERIFY(settings.value(QStringLiteral("Autolock")).toBool());
  QCOMPARE(settings.value(QStringLiteral("Timeout")).toInt(), 42);
}

void ScreenLockSettingsTest::hugeFiniteTimeoutIsClampedBeforeConversion() {
  QTemporaryDir directory;
  QVERIFY(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("kscreenlockerrc"));
  QFile file(path);
  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("[Daemon]\nAutolock=true\nTimeout=1e300\n");
  file.close();

  IniScreenLockPreferencesStore store(path);
  ScreenLockPreferences preferences;
  QString error;
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.timeoutMinutes,
           static_cast<double>(ScreenLockSettingsModel::MaximumTimeoutMinutes));

  QVERIFY(file.open(QIODevice::WriteOnly));
  file.write("[Daemon]\nAutolock=true\nTimeout=-1e300\n");
  file.close();
  QVERIFY(store.load(&preferences, &error));
  QCOMPARE(preferences.timeoutMinutes,
           static_cast<double>(ScreenLockSettingsModel::MinimumTimeoutMinutes));
}

QTEST_GUILESS_MAIN(ScreenLockSettingsTest)
#include "tst_screen_lock_settings.moc"
