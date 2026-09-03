// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_color/color_settings_model.h>

#include "color_settings_projection.h"

#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsColor {
namespace {

using QindaQt::DisplayClient::ClientState;
using QindaQt::DisplayColor::ApplyStatus;
using QindaQt::DisplayColor::AssignmentApplyOutcome;
using QindaQt::DisplayColor::ColorAssignmentDraft;
using QindaQt::DisplayColor::DocumentAvailability;
using QindaQt::DisplayColor::IccProfileDescriptor;
using QindaQt::DisplayColor::ImportResult;
using QindaQt::DisplayColor::ImportStatus;

} // namespace

ColorSettingsModel::ColorSettingsModel(
    QindaQt::DisplayClient::Client &displayClient,
    QindaQt::Services::SettingsClient::SettingsClient &settingsClient,
    QindaQt::DisplayColor::SettingsAssignmentStore &store,
    QindaQt::DisplayColor::ProfileDiscovery &discovery, QObject *parent)
    : QObject(parent), m_displayClient(displayClient),
      m_settingsClient(settingsClient), m_store(store),
      m_discovery(discovery) {
  connect(&m_displayClient, &QindaQt::DisplayClient::Client::stateChanged, this,
          [this](ClientState, const QString &) { synchronizeAuthority(); });
  connect(&m_displayClient, &QindaQt::DisplayClient::Client::snapshotChanged,
          this, [this](const QindaQt::Display::Snapshot &) {
            synchronizeAuthority();
          });
  connect(&m_settingsClient,
          &QindaQt::Services::SettingsClient::SettingsClient::stateChanged,
          this, [this] { synchronizeAuthority(); });
  connect(&m_settingsClient,
          &QindaQt::Services::SettingsClient::SettingsClient::snapshotChanged,
          this, [this] { synchronizeAuthority(); });
  connect(&m_store, &QindaQt::DisplayColor::SettingsAssignmentStore::documentChanged,
          this, [this] { synchronizeAuthority(); });
  connect(&m_store, &QindaQt::DisplayColor::SettingsAssignmentStore::applyFinished,
          this, &ColorSettingsModel::handleApplyFinished);
}

bool ColorSettingsModel::hasDisplaySnapshot() const noexcept {
  if (m_displayClient.owner().isEmpty() || !m_displayClient.hasSnapshot())
    return false;
  // AGENT-CONTRACT: the public client publishes only validated snapshots, so
  // exact lineage fencing here is owner + epoch + revision.
  const auto snapshot = m_displayClient.snapshot();
  return snapshot.has_value() && snapshot->revision != 0
      && !snapshot->serviceEpoch.isEmpty();
}

bool ColorSettingsModel::usableDisplayLineage() const noexcept {
  if (!hasDisplaySnapshot()) return false;
  const auto state = m_displayClient.state();
  return state == ClientState::Ready || state == ClientState::Degraded
      || state == ClientState::Busy;
}

bool ColorSettingsModel::loading() const noexcept {
  // Loading means "no presentable truth yet while a service connects". Once
  // any truth exists (a display snapshot, a confirmed or retained document),
  // the page reports that state instead; a stale retained document outranks
  // the settings client's reconnect retries.
  if (m_displayClient.state() == ClientState::Starting) return true;
  return m_settingsClient.state()
             == QindaQt::Services::SettingsClient::ClientState::Authenticating
      && m_store.document().availability == DocumentAvailability::Unavailable
      && !stale();
}

bool ColorSettingsModel::ready() const noexcept {
  return usableDisplayLineage() && m_displayClient.state() == ClientState::Ready
      && m_store.document().availability == DocumentAvailability::Ready
      && m_catalogScanned && m_catalog.complete;
}

bool ColorSettingsModel::stale() const noexcept {
  // AGENT-NOTE: Settings1 retains the last confirmed document across
  // authority loss (the store keeps it but reports Unavailable). Displayed
  // assignments from that retained document are stale presentation truth,
  // never live authority: controls stay closed until Ready returns.
  return m_store.document().availability == DocumentAvailability::Unavailable
      && !m_store.document().document.records.isEmpty();
}

bool ColorSettingsModel::degraded() const noexcept {
  if (loading() || ready() || stale()) return false;
  // Degraded requires live or confirmed service truth with a bounded
  // limitation; the local catalog alone never lifts the page out of
  // unavailable.
  const auto availability = m_store.document().availability;
  return usableDisplayLineage() || availability != DocumentAvailability::Unavailable;
}

bool ColorSettingsModel::unavailable() const noexcept {
  return !loading() && !ready() && !degraded() && !stale();
}

bool ColorSettingsModel::busy() const noexcept { return m_store.writeInFlight(); }

bool ColorSettingsModel::retryAvailable() const noexcept { return !busy(); }

bool ColorSettingsModel::catalogComplete() const noexcept {
  return m_catalogScanned && m_catalog.complete;
}

bool ColorSettingsModel::unassignAvailable() const {
  return unassignAdmission().isEmpty();
}

QString ColorSettingsModel::statusText() const {
  if (loading()) return tr("Connecting to the display and settings services…");
  if (stale())
    return tr("Color assignment storage is stale while the settings service recovers. Controls are unavailable.");
  if (ready()) return tr("Authoritative display color state is shown.");
  if (degraded()) {
    if (m_store.document().availability == DocumentAvailability::UnusableDocument)
      return tr("The stored color assignments cannot be read safely; assignment controls are disabled.");
    if (m_store.document().availability != DocumentAvailability::Ready)
      return tr("Color assignment storage is unavailable; only the inventory is shown.");
    if (!m_catalogScanned || !m_catalog.complete)
      return tr("The profile catalog is incomplete; only currently admitted controls are enabled.");
    return tr("Display color information is limited; only currently admitted controls are enabled.");
  }
  return tr("The display and settings services are unavailable.");
}

QString ColorSettingsModel::catalogSummaryText() const {
  if (!m_catalogScanned)
    return tr("The profile catalog has not been scanned yet.");
  QString text = tr("%1 color profiles discovered.").arg(m_catalog.profiles.size());
  if (!m_catalog.complete)
    text += QLatin1Char(' ')
        + tr("The scan reached a bound; some profiles may be missing.");
  if (!m_catalog.diagnostics.isEmpty())
    text += QLatin1Char(' ')
        + tr("%1 files were skipped.").arg(m_catalog.diagnostics.size());
  return text;
}

QString ColorSettingsModel::displayEpoch() const {
  return hasDisplaySnapshot() ? m_displayClient.snapshot()->serviceEpoch
                              : QString{};
}

qulonglong ColorSettingsModel::displayRevision() const {
  return hasDisplaySnapshot() ? m_displayClient.snapshot()->revision : 0;
}

qulonglong ColorSettingsModel::settingsRevision() const {
  const auto view = m_store.document();
  return view.availability == DocumentAvailability::Unavailable ? 0 : view.revision;
}

QString ColorSettingsModel::selectedOutputName() const {
  if (!hasDisplaySnapshot() || m_selectedOutputId.isEmpty()) return {};
  // AGENT-GUARD: Copy the snapshot before iterating; ranging over
  // snapshot()->outputs would bind references to a temporary.
  const auto snapshot = m_displayClient.snapshot();
  for (const QindaQt::Display::Output &output : snapshot->outputs)
    if (output.stableId == m_selectedOutputId)
      return Projection::outputDisplayName(output);
  return {};
}

QString ColorSettingsModel::assignedProfileFor(const QString &stableId) const {
  for (const auto &record : m_store.document().document.records)
    if (record.outputStableId == stableId) return record.profileId;
  return {};
}

const IccProfileDescriptor *
ColorSettingsModel::findProfile(const QString &profileId) const {
  for (const auto &profile : m_catalog.profiles)
    if (profile.descriptor.profileId == profileId) return &profile.descriptor;
  return nullptr;
}

QVariantList ColorSettingsModel::outputRows() const {
  if (!hasDisplaySnapshot()) return {};
  QVariantList rows = Projection::outputs(*m_displayClient.snapshot(),
                                          m_store.document().document,
                                          m_catalog);
  for (QVariant &value : rows) {
    QVariantMap row = value.toMap();
    row.insert(QStringLiteral("selected"),
               row.value(QStringLiteral("id")).toString() == m_selectedOutputId);
    value = row;
  }
  return rows;
}

QVariantList ColorSettingsModel::profileRows() const {
  QVariantList rows;
  if (!m_catalogScanned || m_selectedOutputId.isEmpty()) return rows;
  rows = Projection::profiles(m_catalog, assignedProfileFor(m_selectedOutputId));
  for (QVariant &value : rows) {
    QVariantMap row = value.toMap();
    row.insert(QStringLiteral("available"),
               assignAdmission(row.value(QStringLiteral("id")).toString())
                   .isEmpty());
    value = row;
  }
  return rows;
}

QVariantList ColorSettingsModel::inactiveAssignmentRows() const {
  if (!hasDisplaySnapshot()) return {};
  return Projection::inactiveAssignments(*m_displayClient.snapshot(),
                                         m_store.document().document,
                                         m_catalog);
}

QString ColorSettingsModel::assignmentAdmissionBase() const {
  if (m_store.writeInFlight()) return QStringLiteral("write-in-flight");
  const auto displayState = m_displayClient.state();
  if (!usableDisplayLineage()
      || (displayState != ClientState::Ready && displayState != ClientState::Degraded))
    return QStringLiteral("display-unavailable");
  const auto availability = m_store.document().availability;
  if (availability == DocumentAvailability::UnusableDocument)
    return QStringLiteral("document-unusable");
  if (availability != DocumentAvailability::Ready)
    return QStringLiteral("settings-unavailable");
  return {};
}

QString ColorSettingsModel::assignAdmission(const QString &profileId) const {
  const QString base = assignmentAdmissionBase();
  if (!base.isEmpty()) return base;
  if (m_selectedOutputId.isEmpty()) return QStringLiteral("no-output-selected");
  bool outputKnown = false;
  const auto snapshot = m_displayClient.snapshot();
  for (const QindaQt::Display::Output &output : snapshot->outputs)
    if (output.stableId == m_selectedOutputId) outputKnown = true;
  if (!outputKnown) return QStringLiteral("unknown-output");
  if (findProfile(profileId) == nullptr) return QStringLiteral("unknown-profile");
  if (assignedProfileFor(m_selectedOutputId) == profileId)
    return QStringLiteral("already-assigned");
  return {};
}

QString ColorSettingsModel::unassignAdmission() const {
  const QString base = assignmentAdmissionBase();
  if (!base.isEmpty()) return base;
  if (m_selectedOutputId.isEmpty()) return QStringLiteral("no-output-selected");
  if (assignedProfileFor(m_selectedOutputId).isEmpty())
    return QStringLiteral("not-assigned");
  return {};
}

bool ColorSettingsModel::selectOutput(const QString &stableId) {
  if (!hasDisplaySnapshot()) {
    reject(QStringLiteral("display-unavailable"));
    return false;
  }
  const auto snapshot = m_displayClient.snapshot();
  for (const QindaQt::Display::Output &output : snapshot->outputs) {
    if (output.stableId == stableId) {
      if (m_selectedOutputId != stableId) {
        m_selectedOutputId = stableId;
        Q_EMIT viewChanged();
      }
      return true;
    }
  }
  reject(QStringLiteral("unknown-output"));
  return false;
}

bool ColorSettingsModel::submitDraft(const ColorAssignmentDraft &draft,
                                     const QString &progressText) {
  QString error;
  if (!m_store.applyDraft(draft, &error)) {
    reject(error);
    return false;
  }
  m_errorText.clear();
  m_operationStatusText = progressText;
  Q_EMIT viewChanged();
  return true;
}

bool ColorSettingsModel::assignProfile(const QString &profileId) {
  const QString reason = assignAdmission(profileId);
  if (!reason.isEmpty()) {
    reject(reason);
    return false;
  }
  const IccProfileDescriptor *profile = findProfile(profileId);
  // AGENT-GUARD: admission already proved catalog membership; the lineage
  // fingerprint is the import digest when one exists, empty otherwise.
  if (profile == nullptr) {
    reject(QStringLiteral("unknown-profile"));
    return false;
  }
  ColorAssignmentDraft draft;
  draft.entries.append({m_selectedOutputId, profileId,
                        m_importLineageByProfileId.value(
                            profileId, profile->checksumSha256),
                        false});
  return submitDraft(draft, tr("Saving the color assignment…"));
}

bool ColorSettingsModel::unassignSelected() {
  const QString reason = unassignAdmission();
  if (!reason.isEmpty()) {
    reject(reason);
    return false;
  }
  ColorAssignmentDraft draft;
  draft.entries.append({m_selectedOutputId, QString{}, QByteArray{}, true});
  return submitDraft(draft, tr("Removing the color assignment…"));
}

void ColorSettingsModel::handleApplyFinished(
    const AssignmentApplyOutcome &outcome) {
  m_operationStatusText.clear();
  switch (outcome.status) {
  case ApplyStatus::Applied:
    m_errorText.clear();
    m_operationStatusText = tr("The color assignment was saved.");
    break;
  case ApplyStatus::AppliedNoOp:
    m_errorText.clear();
    m_operationStatusText = tr("The color assignment was already up to date.");
    break;
  case ApplyStatus::Conflict:
    m_errorText = tr("Color settings changed elsewhere. The latest authoritative state is shown; the change was not replayed.");
    break;
  case ApplyStatus::Uncertain:
    m_errorText = tr("The color assignment could not be confirmed. It was not replayed.");
    break;
  case ApplyStatus::Failed:
    m_errorText = tr("The color assignment failed (%1).").arg(outcome.reasonCode);
    break;
  }
  Q_EMIT viewChanged();
}

bool ColorSettingsModel::importProfile(const QUrl &source) {
  if (!source.isLocalFile() || source.toLocalFile().isEmpty()) {
    m_importStatusText = tr("Only local profile files can be imported.");
    Q_EMIT viewChanged();
    return false;
  }
  const ImportResult result =
      m_discovery.importUserProfile(source.toLocalFile());
  const QString name = result.profile.descriptor.displayName;
  if (result.imported()) {
    // AGENT-GUARD: A rescan can never recover the import digest (discovery
    // deliberately never reads profile bodies), so the draft lineage
    // fingerprint is retained from the import result itself. Profiles
    // imported before this session carry an empty lineage, exactly as the
    // C1 document contract records ("predates a verified import").
    m_importLineageByProfileId.insert(result.profile.descriptor.profileId,
                                      result.profile.descriptor.checksumSha256);
    refreshCatalog();
    m_importStatusText = result.status == ImportStatus::AlreadyPresent
        ? tr("The profile %1 was already imported.").arg(name)
        : tr("Imported the profile %1.").arg(name);
    Q_EMIT viewChanged();
    return true;
  }
  switch (result.status) {
  case ImportStatus::InvalidSource:
    m_importStatusText = tr("That file is not a valid ICC profile.");
    break;
  case ImportStatus::SourceUnreadable:
    m_importStatusText = tr("That file could not be read.");
    break;
  case ImportStatus::SourceOversized:
    m_importStatusText = tr("That file is larger than the 4 MiB profile limit.");
    break;
  case ImportStatus::SourceIsSymlink:
    m_importStatusText = tr("Symbolic links cannot be imported.");
    break;
  case ImportStatus::SourceNameUnsafe:
    m_importStatusText = tr("That file name is not safe to store as a profile.");
    break;
  case ImportStatus::InvalidUserRoot:
    m_importStatusText = tr("The user profile directory is not usable.");
    break;
  case ImportStatus::DestinationConflict:
    m_importStatusText = tr("A different profile already uses that file name.");
    break;
  case ImportStatus::WriteFailed:
    m_importStatusText = tr("The profile could not be stored.");
    break;
  case ImportStatus::DurabilityUncertain:
    m_importStatusText = tr("The profile was copied, but its durability could not be confirmed.");
    break;
  case ImportStatus::Imported:
  case ImportStatus::AlreadyPresent:
    break;
  }
  Q_EMIT viewChanged();
  return false;
}

bool ColorSettingsModel::retry() {
  if (!retryAvailable()) {
    reject(QStringLiteral("write-in-flight"));
    return false;
  }
  m_retrying = true;
  m_displayClient.refresh();
  m_settingsClient.refresh();
  refreshCatalog();
  m_errorText.clear();
  m_operationStatusText = tr("Refreshing display color state…");
  Q_EMIT viewChanged();
  return true;
}

void ColorSettingsModel::setRouteActive(const bool active) {
  m_routeActive = active;
  // AGENT-NOTE: Discovery is bounded and synchronous over injected roots, so
  // each activation rescans; imported or removed profiles appear on return.
  if (active) refreshCatalog();
  Q_EMIT viewChanged();
}

void ColorSettingsModel::refreshCatalog() {
  m_catalog = m_discovery.discoverCatalog();
  m_catalogScanned = true;
  Q_EMIT viewChanged();
}

void ColorSettingsModel::synchronizeAuthority() {
  if (m_retrying && (ready() || degraded())) {
    m_retrying = false;
    m_operationStatusText.clear();
  }
  if (!m_selectedOutputId.isEmpty()) {
    bool stillPresent = false;
    if (hasDisplaySnapshot()) {
      const auto snapshot = m_displayClient.snapshot();
      for (const QindaQt::Display::Output &output : snapshot->outputs)
        if (output.stableId == m_selectedOutputId) stillPresent = true;
    }
    if (!stillPresent) m_selectedOutputId.clear();
  }
  Q_EMIT viewChanged();
}

void ColorSettingsModel::reject(const QString &reason) {
  m_operationStatusText.clear();
  m_errorText = failureText(reason);
  Q_EMIT actionRejected(reason);
  Q_EMIT viewChanged();
}

QString ColorSettingsModel::failureText(const QString &reason) const {
  if (reason == QStringLiteral("write-in-flight"))
    return tr("Another color change is still being saved.");
  if (reason == QStringLiteral("display-unavailable"))
    return tr("The display inventory is unavailable.");
  if (reason == QStringLiteral("settings-unavailable")
      || reason == QStringLiteral("unavailable"))
    return stale()
        ? tr("Color assignment storage is stale while the settings service recovers.")
        : tr("The settings service is unavailable.");
  if (reason == QStringLiteral("document-unusable")
      || reason.startsWith(QStringLiteral("document-unusable/")))
    return tr("The stored color assignments cannot be read safely.");
  if (reason == QStringLiteral("no-output-selected"))
    return tr("Select a display first.");
  if (reason == QStringLiteral("unknown-output"))
    return tr("That display is no longer connected.");
  if (reason == QStringLiteral("unknown-profile"))
    return tr("That profile is no longer in the discovered catalog.");
  if (reason == QStringLiteral("already-assigned"))
    return tr("That profile is already assigned to this display.");
  if (reason == QStringLiteral("not-assigned"))
    return tr("This display has no assigned profile.");
  if (reason.startsWith(QStringLiteral("invalid-draft")))
    return tr("That color assignment is not valid.");
  return reason.isEmpty() ? tr("The color request was rejected.")
                          : tr("The color request failed (%1).").arg(reason);
}

} // namespace QindaQt::Apps::SettingsColor
