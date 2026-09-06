// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <memory>

namespace QindaQt::Apps::SettingsPower {

struct ScreenLockPreferences final {
  bool automaticLock = true;
  double timeoutMinutes = 5.0;
};

// Local-file boundary for KScreenLocker's documented kscreenlockerrc Daemon
// values. Implementations may preserve every unrelated group/key.
class ScreenLockPreferencesStore {
public:
  virtual ~ScreenLockPreferencesStore() = default;
  [[nodiscard]] virtual bool load(ScreenLockPreferences *preferences,
                                  QString *error) = 0;
  [[nodiscard]] virtual bool save(const ScreenLockPreferences &preferences,
                                  QString *error) = 0;
};

class IniScreenLockPreferencesStore final : public ScreenLockPreferencesStore {
public:
  explicit IniScreenLockPreferencesStore(QString filePath);
  [[nodiscard]] bool load(ScreenLockPreferences *preferences,
                          QString *error) override;
  [[nodiscard]] bool save(const ScreenLockPreferences &preferences,
                          QString *error) override;

private:
  QString m_filePath;
};

// The live reload request stays behind an injected adapter. A failed reload
// does not roll back the persisted user choice; the model reports that split
// truth and never claims that the running locker adopted the preference.
class ScreenLockConfigureClient : public QObject {
  Q_OBJECT
public:
  using QObject::QObject;
  ~ScreenLockConfigureClient() override = default;
  virtual void requestConfigure() = 0;

Q_SIGNALS:
  void configured(bool accepted, const QString &error);
};

class QtScreenLockConfigureClient final : public ScreenLockConfigureClient {
  Q_OBJECT
public:
  explicit QtScreenLockConfigureClient(QObject *parent = nullptr);
  void requestConfigure() override;
};

class ScreenLockSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool automaticLock READ automaticLock NOTIFY changed)
  Q_PROPERTY(int timeoutMinutes READ timeoutMinutes NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)

public:
  static constexpr int MinimumTimeoutMinutes = 1;
  static constexpr int MaximumTimeoutMinutes = 240;

  ScreenLockSettingsModel(std::unique_ptr<ScreenLockPreferencesStore> store,
                          ScreenLockConfigureClient &configureClient,
                          QObject *parent = nullptr);

  [[nodiscard]] bool automaticLock() const noexcept;
  [[nodiscard]] int timeoutMinutes() const noexcept;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] const QString &statusText() const noexcept;
  [[nodiscard]] const QString &errorText() const noexcept;

  Q_INVOKABLE bool setAutomaticLock(bool enabled);
  Q_INVOKABLE bool setTimeoutMinutes(int minutes);
  Q_INVOKABLE bool retryLiveApply();

Q_SIGNALS:
  void changed();

private:
  enum class PendingSaveField { None, AutomaticLock, Timeout };

  [[nodiscard]] bool reloadLatest(ScreenLockPreferences *preferences,
                                  bool marksLoadFailure = true);
  [[nodiscard]] bool persist(const ScreenLockPreferences &preferences,
                             PendingSaveField pendingField);
  void beginLiveConfigure(const QString &pendingText);

  std::unique_ptr<ScreenLockPreferencesStore> m_store;
  ScreenLockConfigureClient &m_configureClient;
  ScreenLockPreferences m_preferences;
  // AGENT-GUARD: A failed load/save stays failed until the same storage step
  // succeeds. Retry must re-run that step; only a persisted pair may reach the
  // live configure request, so a success signal can never describe an unsaved
  // change.
  bool m_loadFailed = false;
  bool m_savePending = false;
  // AGENT-GUARD: Save retry must merge its one intended field onto a freshly
  // loaded pair. Retrying a stale pair would reintroduce the external-edit
  // loss that the initial mutation avoids.
  PendingSaveField m_pendingSaveField = PendingSaveField::None;
  ScreenLockPreferences m_pendingPreferences;
  bool m_busy = false;
  QString m_statusText;
  QString m_errorText;
};

} // namespace QindaQt::Apps::SettingsPower
