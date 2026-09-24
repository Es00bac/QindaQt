// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_screensaver/screensaver_settings_model.h>

#include <qindaqt/session/desktop_controls/screensaver_catalog.h>

#include <QVariant>
#include <QStringList>

#include <algorithm>

namespace QindaQt::Apps::SettingsScreensaver {
namespace {

using Session::DesktopControls::ScreensaverPreferences;

const QString SaverKey = QStringLiteral("power.screensaver");
const QString MinutesKey = QStringLiteral("power.screensaverMinutes");
constexpr int ReadbackRetryMilliseconds = 200;
constexpr int ReadbackDeadlineMilliseconds = 4'000;

bool exactMinutes(const QVariant &value, int *minutes) {
  const int type = value.metaType().id();
  if (type != QMetaType::Int && type != QMetaType::UInt
      && type != QMetaType::LongLong && type != QMetaType::ULongLong) {
    return false;
  }
  bool ok = false;
  const qlonglong number = value.toLongLong(&ok);
  if (!ok || number < 1
      || number > ScreensaverPreferences::maximumTimeoutMinutes()) {
    return false;
  }
  *minutes = int(number);
  return true;
}

} // namespace

using Session::DesktopControls::ScreensaverCatalogEntry;
using Session::DesktopControls::Settings1ScreensaverPreferences;
using Services::SettingsClient::ClientState;
using Services::SettingsProtocol::SettingsWireStatus;

ScreensaverSettingsModel::ScreensaverSettingsModel(
    Settings1ScreensaverPreferences &preferences,
    Services::SettingsClient::SettingsClient &client,
    const Session::DesktopControls::ScreensaverCatalog &catalog,
    LockScreenSaverStore &lockScreenSaver, ScreensaverPreview &preview,
    QObject *parent)
    : QObject(parent), m_preferences(preferences), m_client(client),
      m_catalog(catalog), m_lockScreenSaver(lockScreenSaver),
      m_preview(preview) {
  // AGENT-CONTRACT: the provider has a safe runtime fallback, not UI proof.
  // Even a first Settings1 snapshot equal to that fallback must establish
  // route authority; the provider emits no preferencesChanged in that case.
  connect(&m_client, &Services::SettingsClient::SettingsClient::snapshotChanged,
          this, &ScreensaverSettingsModel::handleSnapshot);
  connect(&m_client, &Services::SettingsClient::SettingsClient::stateChanged,
          this, &ScreensaverSettingsModel::handleClientState);
  connect(&m_client, &Services::SettingsClient::SettingsClient::ownerChanged,
          this, &ScreensaverSettingsModel::handleClientState);
  connect(&m_client, &Services::SettingsClient::SettingsClient::writeAdmissionChanged,
          this, &ScreensaverSettingsModel::publishStatus);
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitFinished,
          this, &ScreensaverSettingsModel::handleCommit);
  connect(&m_client, &Services::SettingsClient::SettingsClient::commitUncertain,
          this, &ScreensaverSettingsModel::handleUncertain);
  connect(&m_preview, &ScreensaverPreview::finished, this,
          &ScreensaverSettingsModel::publishStatus);
  connect(&m_preview, &ScreensaverPreview::failed, this,
          [this](const QString &message) {
            m_previewError = message;
            publishStatus();
          });
  m_readbackRetryTimer.setSingleShot(true);
  connect(&m_readbackRetryTimer, &QTimer::timeout, this, [this] {
    if (m_pending && m_waitingForReadback) m_client.refresh();
  });
  m_readbackDeadlineTimer.setSingleShot(true);
  connect(&m_readbackDeadlineTimer, &QTimer::timeout, this, [this] {
    if (!m_pending || !m_waitingForReadback) return;
    // AGENT-GUARD: a valid but pre-commit snapshot can leave SettingsClient
    // Ready with no automatic follow-up. End the pending UI without replaying
    // the write when the service never supplies the Applied revision.
    retirePending(tr("The saved screen saver choice could not be confirmed in time."));
    publishStatus();
  });
  if (m_client.state() == ClientState::Ready && m_client.snapshot())
    handleSnapshot();
  else
    publishStatus();
}

ScreensaverSettingsModel::~ScreensaverSettingsModel() = default;

QVariantList ScreensaverSettingsModel::saverOptions() const {
  QVariantList options;
  options.append(QVariantMap{
      {QStringLiteral("token"), ScreensaverPreferences::noneToken()},
      {QStringLiteral("name"), tr("None")},
      {QStringLiteral("comment"), tr("Nothing runs when the session is idle.")},
      {QStringLiteral("iconName"), QStringLiteral("dialog-cancel")},
      {QStringLiteral("showsOnLockScreen"), false},
  });
  options.append(QVariantMap{
      {QStringLiteral("token"), ScreensaverPreferences::blankToken()},
      {QStringLiteral("name"), tr("Blank screen")},
      {QStringLiteral("comment"),
       tr("Nothing runs while the session is unlocked; the lock screen shows "
          "a plain dark screen.")},
      {QStringLiteral("iconName"), QStringLiteral("view-hidden")},
      {QStringLiteral("showsOnLockScreen"), true},
  });
  QList<ScreensaverCatalogEntry> discovered = m_catalog.entries();
  std::sort(discovered.begin(), discovered.end(),
            [](const ScreensaverCatalogEntry &left,
               const ScreensaverCatalogEntry &right) {
              return QString::compare(left.name, right.name,
                                      Qt::CaseInsensitive) < 0;
            });
  for (const ScreensaverCatalogEntry &entry : discovered) {
    options.append(QVariantMap{
        {QStringLiteral("token"), entry.token},
        {QStringLiteral("name"), entry.name},
        {QStringLiteral("comment"), entry.comment},
        {QStringLiteral("iconName"), entry.iconName},
        {QStringLiteral("showsOnLockScreen"), entry.showsOnLockScreen},
    });
  }
  return options;
}

QString ScreensaverSettingsModel::saver() const {
  return m_confirmed.has_value() ? m_confirmed->saver : QString{};
}

bool ScreensaverSettingsModel::delayEnabled() const {
  return m_confirmed.has_value() && m_confirmed->enabled();
}

int ScreensaverSettingsModel::minutes() const {
  return m_confirmed.has_value() ? m_confirmed->minutes : 0;
}

bool ScreensaverSettingsModel::hasConfirmed() const noexcept {
  return m_confirmed.has_value();
}

bool ScreensaverSettingsModel::available() const noexcept { return m_available; }

bool ScreensaverSettingsModel::canEdit() const {
  return m_available && !m_pending
         && m_client.canSetUserValue(SaverKey)
         && m_client.canSetUserValue(MinutesKey);
}

bool ScreensaverSettingsModel::busy() const noexcept { return m_pending; }
bool ScreensaverSettingsModel::conflict() const noexcept { return m_conflict; }
bool ScreensaverSettingsModel::uncertain() const noexcept { return m_uncertain; }

const QString &ScreensaverSettingsModel::statusText() const noexcept {
  return m_statusText;
}

QString ScreensaverSettingsModel::errorText() const {
  QStringList messages;
  if (!m_localError.isEmpty()) messages.append(m_localError);
  if (!m_mirrorError.isEmpty()) messages.append(m_mirrorError);
  if (!m_schemaError.isEmpty()) messages.append(m_schemaError);
  return messages.join(QLatin1Char('\n'));
}

bool ScreensaverSettingsModel::previewAvailable() const {
  return m_available && m_confirmed.has_value()
         && m_preview.kindFor(m_confirmed->saver)
                != ScreensaverPreview::Kind::Unavailable;
}

bool ScreensaverSettingsModel::previewRunning() const {
  return m_preview.running();
}

const QString &ScreensaverSettingsModel::previewSummary() const noexcept {
  return m_previewSummary;
}

const QString &ScreensaverSettingsModel::previewErrorText() const noexcept {
  return m_previewError;
}

QString ScreensaverSettingsModel::displayName(const QString &saver) const {
  if (saver == ScreensaverPreferences::blankToken()) return tr("Blank screen");
  const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(saver);
  if (entry.has_value() && !entry->name.isEmpty()) return entry->name;
  return saver;
}

bool ScreensaverSettingsModel::setSaver(const QString &saver) {
  if (m_pending) return false;
  // The catalog is the only path from a persisted token to a program.
  if (saver != ScreensaverPreferences::noneToken()
      && saver != ScreensaverPreferences::blankToken()
      && !m_catalog.entry(saver).has_value()) {
    m_localError = tr("That screensaver is not available.");
    publishStatus();
    return false;
  }
  // A normalized unknown token can display as None while the raw key still
  // needs repair; compare the authoritative wire value for no-op admission.
  if (canEdit() && m_client.snapshot()->values.value(SaverKey).toString() == saver)
    return true;
  return submit(SaverKey, saver);
}

bool ScreensaverSettingsModel::setMinutes(int minutes) {
  if (m_pending) return false;
  if (minutes < 1 || minutes > ScreensaverPreferences::maximumTimeoutMinutes()) {
    m_localError = tr("Choose a delay between 1 and %1 minutes.")
                       .arg(ScreensaverPreferences::maximumTimeoutMinutes());
    publishStatus();
    return false;
  }
  if (canEdit() && m_client.snapshot()->values.value(MinutesKey).toLongLong()
                       == minutes)
    return true;
  return submit(MinutesKey, QVariant::fromValue(static_cast<qint64>(minutes)));
}

bool ScreensaverSettingsModel::retry() {
  if (m_pending) return false;
  // Retry re-reads only. An uncertain write may already have persisted and
  // must never be sent again without a fresh explicit user selection.
  // An unchanged refresh cannot resolve a refused or uncertain write.
  // Keep its diagnostic until a fresh user choice earns a matching readback.
  m_preferences.refresh();
  publishStatus();
  return true;
}

bool ScreensaverSettingsModel::preview() {
  if (m_pending || !previewAvailable()) {
    m_localError = tr("Wait for confirmed screen saver settings before previewing.");
    publishStatus();
    return false;
  }
  m_previewError.clear();
  QString error;
  if (!m_preview.start(saver(), &error)) {
    m_previewError = error.isEmpty() ? tr("The preview could not be started.")
                                     : error;
    publishStatus();
    return false;
  }
  publishStatus();
  return true;
}

void ScreensaverSettingsModel::refreshSavers() {
  Q_EMIT saversChanged();
  publishStatus();
}

bool ScreensaverSettingsModel::submit(const QString &key, const QVariant &value) {
  if (!canEdit()) {
    m_localError = tr("Screen saver settings are not ready for changes.");
    publishStatus();
    return false;
  }
  const auto &snapshot = m_client.snapshot();
  m_writeOwner = m_client.currentOwner();
  m_writeEpoch = snapshot->epoch;
  m_writeKey = key;
  m_requestedValue = value;
  m_pending = true;
  m_waitingForReadback = false;
  m_readbackRevision = 0;
  m_conflict = false;
  m_uncertain = false;
  m_localError.clear();
  QString error;
  if (!m_client.setUserValue(key, value, &error)) {
    clearPending();
    m_localError = error.isEmpty() ? tr("Could not save the screensaver preference.")
                                   : error.left(512);
    publishStatus();
    return false;
  }
  publishStatus();
  return true;
}

void ScreensaverSettingsModel::clearPending() {
  m_readbackRetryTimer.stop();
  m_readbackDeadlineTimer.stop();
  m_pending = false;
  m_waitingForReadback = false;
  m_writeOwner.clear();
  m_writeEpoch.clear();
  m_writeKey.clear();
  m_requestedValue.clear();
  m_readbackRevision = 0;
}

void ScreensaverSettingsModel::retirePending(const QString &message) {
  clearPending();
  m_uncertain = true;
  m_localError = message;
}

void ScreensaverSettingsModel::handleSnapshot() {
  const auto &snapshot = m_client.snapshot();
  if (!snapshot || snapshot->owner != m_client.currentOwner()
      || m_client.state() != ClientState::Ready) {
    handleClientState();
    return;
  }
  const QVariant saverValue = snapshot->values.value(SaverKey);
  const QVariant minutesValue = snapshot->values.value(MinutesKey);
  int nextMinutes = 0;
  if (saverValue.metaType().id() != QMetaType::QString
      || !exactMinutes(minutesValue, &nextMinutes)) {
    m_available = false;
    m_schemaError = tr("The screen saver settings returned invalid values.");
    if (m_pending && m_waitingForReadback)
      retirePending(tr("The saved screen saver choice could not be confirmed."));
    publishStatus();
    return;
  }
  const ScreensaverPreferences next = ScreensaverPreferences::fromPersisted(
      saverValue.toString(), nextMinutes, m_catalog);
  if (m_pending && m_waitingForReadback
      && snapshot->owner == m_writeOwner
      && snapshot->epoch == m_writeEpoch
      && snapshot->revision < m_readbackRevision) {
    // SettingsClient accepts an unchanged old revision as Ready; the
    // post-commit fetch alone is not evidence of the Applied write.
    m_readbackRetryTimer.start(ReadbackRetryMilliseconds);
  }
  if (m_pending && m_waitingForReadback
      && snapshot->owner == m_writeOwner
      && snapshot->epoch == m_writeEpoch
      && snapshot->revision >= m_readbackRevision) {
    const bool confirmed = snapshot->values.value(m_writeKey) == m_requestedValue;
    clearPending();
    if (confirmed) {
      m_localError.clear();
      m_conflict = false;
      m_uncertain = false;
    } else {
      m_conflict = true;
      m_localError = tr("The saved screen saver choice differs from your selection.");
    }
  }
  m_confirmed = next;
  m_available = true;
  m_schemaError.clear();
  // A fresh confirmed snapshot may carry an external change. Mirroring it
  // belongs to this route, but a prior write error is not its diagnostic.
  mirrorToLockScreen(next.saver);
  publishStatus();
}

void ScreensaverSettingsModel::handleClientState() {
  if (m_pending
      && (m_client.currentOwner() != m_writeOwner
          || m_client.state() == ClientState::Unavailable
          || m_client.state() == ClientState::Degraded)) {
    retirePending(tr("The screen saver save could not be confirmed after the service changed."));
  }
  // Only handleSnapshot may establish editable authority, including when a
  // new baseline equals the provider's disabled runtime fallback.
  m_available = false;
  publishStatus();
}

void ScreensaverSettingsModel::handleCommit(
    const Services::SettingsClient::CommitOutcome &outcome) {
  if (!m_pending || m_waitingForReadback) return;
  if (outcome.status == SettingsWireStatus::Applied) {
    m_waitingForReadback = true;
    m_readbackRevision = outcome.revisionAfter;
    m_readbackDeadlineTimer.start(ReadbackDeadlineMilliseconds);
  } else {
    clearPending();
    m_conflict = outcome.status == SettingsWireStatus::Conflict;
    m_localError = outcome.message.isEmpty()
        ? tr("The screen saver change was refused (%1).")
              .arg(Services::SettingsProtocol::settingsWireStatusName(outcome.status))
        : outcome.message.left(512);
  }
  publishStatus();
}

void ScreensaverSettingsModel::handleUncertain(const QString &message) {
  if (!m_pending) return;
  retirePending(message.isEmpty()
                    ? tr("The screen saver save result is unknown.")
                    : message.left(512));
  publishStatus();
}

void ScreensaverSettingsModel::mirrorToLockScreen(const QString &saver) {
  // AGENT-GUARD: only a saver the greeter can draw may take the lock
  // wallpaper over (ADR-0216). "blank" is the deliberate painted exception.
  QString mirrored = ScreensaverPreferences::noneToken();
  if (saver == ScreensaverPreferences::blankToken()) {
    mirrored = ScreensaverPreferences::blankToken();
  } else {
    const std::optional<ScreensaverCatalogEntry> entry = m_catalog.entry(saver);
    if (entry.has_value() && entry->showsOnLockScreen) mirrored = saver;
  }
  if (m_lockScreenSaver.currentSaver() == mirrored) {
    m_mirrorError.clear();
    return;
  }
  QString error;
  if (!m_lockScreenSaver.save(mirrored, &error)) {
    m_mirrorError = tr("The screen saver was saved, but the lock screen could "
                       "not be told about it: %1")
                        .arg(error.isEmpty() ? tr("unknown reason") : error);
  } else {
    m_mirrorError.clear();
  }
}

void ScreensaverSettingsModel::publishStatus() {
  if (!m_confirmed) {
    m_statusText = m_client.currentOwner().isEmpty()
        ? tr("Screen saver settings are unavailable; no choice has been confirmed.")
        : tr("Waiting for confirmed screen saver settings.");
    publishPreviewSummary();
    Q_EMIT changed();
    return;
  }
  QString confirmed;
  if (m_confirmed->saver == ScreensaverPreferences::blankToken()) {
    confirmed = tr("Nothing runs while the session is unlocked. The lock "
                   "screen shows a plain dark screen.");
  } else if (!m_confirmed->enabled()) {
    confirmed = tr("No screensaver starts when the session is idle.");
  } else {
    const auto entry = m_catalog.entry(m_confirmed->saver);
    confirmed = entry.has_value() && entry->showsOnLockScreen
        ? tr("%1 starts after %n minute(s) of inactivity, and keeps "
             "showing while the screen is locked.", nullptr, m_confirmed->minutes)
              .arg(displayName(m_confirmed->saver))
        : tr("%1 starts after %n minute(s) of inactivity. The lock "
             "screen keeps its own wallpaper.", nullptr, m_confirmed->minutes)
              .arg(displayName(m_confirmed->saver));
  }
  if (m_pending) {
    m_statusText = (m_waitingForReadback
        ? tr("Checking the saved screen saver choice. Last confirmed: %1")
        : tr("Saving the screen saver choice. Last confirmed: %1")).arg(confirmed);
  } else if (!m_available) {
    m_statusText = tr("Current screen saver settings are unavailable. Last confirmed: %1")
                       .arg(confirmed);
  } else if (m_conflict) {
    m_statusText = tr("Screen saver settings changed elsewhere. Current choice: %1")
                       .arg(confirmed);
  } else if (m_uncertain) {
    m_statusText = tr("The save result is unknown. Current confirmed choice: %1")
                       .arg(confirmed);
  } else {
    m_statusText = confirmed;
  }
  publishPreviewSummary();
  Q_EMIT changed();
}

void ScreensaverSettingsModel::publishPreviewSummary() {
  switch (m_preview.kindFor(saver())) {
  case ScreensaverPreview::Kind::BlackWindow:
    m_previewSummary =
        tr("Shows a full-screen black window. Any key, click, or pointer "
           "movement closes it; the session stays unlocked.");
    break;
  case ScreensaverPreview::Kind::SaverProgram:
    m_previewSummary =
        tr("Runs %1 itself with the catalog arguments used while idle. Any "
           "input dismisses the preview; the lock screen is not shown.")
            .arg(displayName(saver()));
    break;
  case ScreensaverPreview::Kind::Unavailable:
    m_previewSummary.clear();
    break;
  }
}

} // namespace QindaQt::Apps::SettingsScreensaver
