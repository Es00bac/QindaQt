// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/apps/settings_screensaver/lock_screen_saver_store.h>
#include <qindaqt/apps/settings_screensaver/screensaver_preview.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/session/desktop_controls/screensaver_preferences.h>
#include <qindaqt/session/desktop_controls/settings1_screensaver_preferences.h>

#include <QObject>
#include <optional>
#include <QString>
#include <QVariant>
#include <QVariantList>

namespace QindaQt::Session::DesktopControls {
class ScreensaverCatalog;
}

namespace QindaQt::Apps::SettingsScreensaver {

// Screen saver route model (ADR-0226). Truth comes from the purpose-scoped
// Settings1 pair `power.screensaver` / `power.screensaverMinutes` through the
// purpose-scoped client. The desktop-controls provider has a runtime safety
// fallback, so the page reads client snapshots directly for UI authority.
// An uncertain commit never fabricates success; only a same-lineage readback
// reconciles the route.
//
// The saver list is discovered from the installed desktop entries through the
// shared catalog, never hard-coded, so a newly packaged saver appears without
// a code change; "none" and "blank" are the two built-in choices. Confirmed
// truth is also mirrored into the screen locker's greeter through the
// injected LockScreenSaverStore, so a locked session keeps showing the saver
// the user chose (ADR-0216) -- including "blank", which is the plugin's plain
// dark ground. That mirror is the only thing this model writes outside
// Settings1, and it never touches the automatic-lock or display-off
// preferences, which keep their own stores.
class ScreensaverSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList saverOptions READ saverOptions NOTIFY saversChanged)
  Q_PROPERTY(QString saver READ saver NOTIFY changed)
  Q_PROPERTY(bool delayEnabled READ delayEnabled NOTIFY changed)
  Q_PROPERTY(int minutes READ minutes NOTIFY changed)
  Q_PROPERTY(bool hasConfirmed READ hasConfirmed NOTIFY changed)
  Q_PROPERTY(bool available READ available NOTIFY changed)
  Q_PROPERTY(bool canEdit READ canEdit NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool conflict READ conflict NOTIFY changed)
  Q_PROPERTY(bool uncertain READ uncertain NOTIFY changed)
  Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
  Q_PROPERTY(QString errorText READ errorText NOTIFY changed)
  Q_PROPERTY(bool previewAvailable READ previewAvailable NOTIFY changed)
  Q_PROPERTY(bool previewRunning READ previewRunning NOTIFY changed)
  Q_PROPERTY(QString previewSummary READ previewSummary NOTIFY changed)

public:
  ScreensaverSettingsModel(
      Session::DesktopControls::Settings1ScreensaverPreferences &preferences,
      Services::SettingsClient::SettingsClient &client,
      const Session::DesktopControls::ScreensaverCatalog &catalog,
      LockScreenSaverStore &lockScreenSaver, ScreensaverPreview &preview,
      QObject *parent = nullptr);
  ~ScreensaverSettingsModel() override;

  ScreensaverSettingsModel(const ScreensaverSettingsModel &) = delete;
  ScreensaverSettingsModel &operator=(const ScreensaverSettingsModel &) = delete;

  // One row per choice: the two built-ins, then every discovered saver. Each
  // row carries token, name, comment, iconName, and showsOnLockScreen.
  [[nodiscard]] QVariantList saverOptions() const;
  [[nodiscard]] QString saver() const;
  // Whether the "start after" delay row is live: a real program is chosen,
  // so not for "none" or "blank".
  [[nodiscard]] bool delayEnabled() const;
  [[nodiscard]] int minutes() const;
  [[nodiscard]] bool hasConfirmed() const noexcept;
  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] bool canEdit() const;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool conflict() const noexcept;
  [[nodiscard]] bool uncertain() const noexcept;
  [[nodiscard]] const QString &statusText() const noexcept;
  [[nodiscard]] QString errorText() const;
  [[nodiscard]] bool previewAvailable() const;
  [[nodiscard]] bool previewRunning() const;
  [[nodiscard]] const QString &previewSummary() const noexcept;

  Q_INVOKABLE bool setSaver(const QString &saver);
  Q_INVOKABLE bool setMinutes(int minutes);
  Q_INVOKABLE bool retry();
  Q_INVOKABLE bool preview();
  // Re-scans the catalog (a saver package (un)installed while the page is
  // open) and re-publishes the option list; persisted truth is untouched.
  Q_INVOKABLE void refreshSavers();

Q_SIGNALS:
  void changed();
  void saversChanged();

private:
  void publishStatus();
  void publishPreviewSummary();
  void handleSnapshot();
  void handleClientState();
  void handleCommit(const Services::SettingsClient::CommitOutcome &outcome);
  void handleUncertain(const QString &message);
  void retirePending(const QString &message);
  void clearPending();
  // Mirrors confirmed truth into the greeter. A failure here is reported as
  // its own error: the preference itself is persisted either way.
  void mirrorToLockScreen(const QString &saver);
  [[nodiscard]] bool submit(const QString &key, const QVariant &value);
  [[nodiscard]] QString displayName(const QString &saver) const;

  Session::DesktopControls::Settings1ScreensaverPreferences &m_preferences;
  Services::SettingsClient::SettingsClient &m_client;
  const Session::DesktopControls::ScreensaverCatalog &m_catalog;
  LockScreenSaverStore &m_lockScreenSaver;
  ScreensaverPreview &m_preview;
  std::optional<Session::DesktopControls::ScreensaverPreferences> m_confirmed;
  QString m_statusText;
  QString m_localError;
  QString m_mirrorError;
  QString m_schemaError;
  QString m_previewSummary;
  QString m_writeOwner;
  QString m_writeEpoch;
  QString m_writeKey;
  QVariant m_requestedValue;
  quint64 m_readbackRevision = 0;
  bool m_available = false;
  bool m_pending = false;
  bool m_waitingForReadback = false;
  bool m_conflict = false;
  bool m_uncertain = false;
};

} // namespace QindaQt::Apps::SettingsScreensaver
