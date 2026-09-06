// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_color/color_settings_model.h>

#include <algorithm>
#include <limits>

namespace QindaQt::Apps::SettingsColor {
namespace {

using QindaQt::DisplayColor::ApplyStatus;
using QindaQt::DisplayColor::AssignmentApplyOutcome;
using QindaQt::DisplayColor::DocumentAvailability;
using QindaQt::DisplayWriter::CompletionOutcome;
using QindaQt::DisplayWriter::PortStartStatus;
using QindaQt::DisplayWriter::SubmitStatus;

} // namespace

ColorSettingsModel::~ColorSettingsModel() {
  if (m_colorPort != nullptr)
    m_colorPort->stop();
}

void ColorSettingsModel::installColorApplicationPort(
    std::unique_ptr<QindaQt::DisplayWriter::OutputManagementPort> port) {
  if (m_colorPort != nullptr)
    m_colorPort->stop();
  m_colorPort = std::move(port);
  m_colorPortAvailable = false;
  m_colorApplyInFlight = false;
  m_reconciliationNeeded = true;
  if (m_colorPort != nullptr) {
    m_colorPort->setObserver(this);
    const PortStartStatus status = m_colorPort->start();
    if (status != PortStartStatus::Started &&
        status != PortStartStatus::AlreadyStarted) {
      m_errorText =
          tr("Color profiles cannot be applied to displays right now.");
    }
  }
  Q_EMIT viewChanged();
}

bool ColorSettingsModel::compositorApplicationSupported() const noexcept {
  return m_colorPort != nullptr && m_colorPortAvailable;
}

bool ColorSettingsModel::displayChangeInProgress() const noexcept {
  return hasDisplaySnapshot() &&
         !m_displayClient.snapshot()->transactions.isEmpty();
}

void ColorSettingsModel::handleApplyFinished(
    const AssignmentApplyOutcome &outcome) {
  m_operationStatusText.clear();
  switch (outcome.status) {
  case ApplyStatus::Applied:
    m_reconciliationNeeded = false;
    m_errorText.clear();
    if (m_pendingColorApplication.has_value()) {
      if (displayChangeInProgress()) {
        m_reconciliationNeeded = true;
        m_operationStatusText =
            tr("The profile was saved and will be applied after the pending display change finishes.");
      } else {
        applyPendingColorProfile();
      }
    } else {
    m_operationStatusText = tr("The color assignment was saved.");
    }
    break;
  case ApplyStatus::AppliedNoOp:
    m_reconciliationNeeded = false;
    m_errorText.clear();
    if (m_pendingColorApplication.has_value()) {
      if (displayChangeInProgress()) {
        m_reconciliationNeeded = true;
        m_operationStatusText =
            tr("The profile was saved and will be applied after the pending display change finishes.");
      } else {
        applyPendingColorProfile();
      }
    } else {
      m_operationStatusText =
          tr("The color assignment was already up to date.");
    }
    break;
  case ApplyStatus::Conflict:
    m_reconciliationNeeded = true;
    m_pendingColorApplication.reset();
    m_errorText =
        tr("Color settings changed elsewhere. The latest authoritative state "
           "is shown; the change was not replayed.");
    break;
  case ApplyStatus::Uncertain:
    m_pendingColorApplication.reset();
    m_errorText =
        tr("The color assignment could not be confirmed. It was not replayed.");
    break;
  case ApplyStatus::Failed:
    m_pendingColorApplication.reset();
    m_errorText =
        tr("The color assignment failed (%1).").arg(outcome.reasonCode);
    break;
  }
  reconcileSavedProfiles();
  Q_EMIT viewChanged();
}

void ColorSettingsModel::applyPendingColorProfile() {
  if (!m_pendingColorApplication.has_value())
    return;
  if (m_colorPort == nullptr) {
    m_pendingColorApplication.reset();
    m_operationStatusText = tr("The color assignment was saved.");
    return;
  }
  if (!m_colorPortAvailable) {
    m_pendingColorApplication.reset();
    m_errorText = tr("The assignment was saved, but the profile could not be "
                     "applied to the display.");
    m_operationStatusText.clear();
    return;
  }

  m_colorRequestId = m_colorRequestId == std::numeric_limits<quint64>::max()
                         ? 1
                         : m_colorRequestId + 1;
  const SubmitStatus submitted = m_colorPort->submitColorProfile(
      {.requestId = m_colorRequestId,
       .connectorName = m_pendingColorApplication->connectorName,
       .iccProfilePath = m_pendingColorApplication->profilePath});
  if (submitted == SubmitStatus::Accepted) {
    m_colorApplyInFlight = true;
    m_operationStatusText = m_pendingColorApplication->profilePath.isEmpty()
                                ? tr("Restoring the standard color profile…")
                                : tr("Applying the color profile…");
    return;
  }
  m_reconciliationQueue.clear();
  m_currentApplyIsReconciliation = false;
  m_pendingColorApplication.reset();
  m_operationStatusText.clear();
  m_errorText = submitted == SubmitStatus::Unsupported
                    ? tr("This display does not support applying ICC profiles.")
                    : tr("The assignment was saved, but the profile could not "
                         "be applied to the display.");
}

void ColorSettingsModel::reconcileSavedProfiles() {
  if (!m_routeActive || busy() || m_colorPort == nullptr ||
      !m_colorPortAvailable || !usableDisplayLineage() ||
      displayChangeInProgress()) {
    return;
  }
  if (m_pendingColorApplication.has_value()) {
    applyPendingColorProfile();
    return;
  }
  if (!m_reconciliationNeeded ||
      m_store.document().availability != DocumentAvailability::Ready ||
      !m_catalogScanned || !m_catalog.complete) {
    return;
  }

  m_reconciliationNeeded = false;
  m_reconciliationQueue.clear();
  const auto snapshot = m_displayClient.snapshot();
  for (const auto &record : m_store.document().document.records) {
    const auto output =
        std::find_if(snapshot->outputs.cbegin(), snapshot->outputs.cend(),
                     [&record](const auto &candidate) {
                       return candidate.stableId == record.outputStableId &&
                              candidate.enabled;
                     });
    const auto *profile = findDiscoveredProfile(record.profileId);
    if (output != snapshot->outputs.cend() && profile != nullptr &&
        !profile->sourcePath.isEmpty()) {
      m_reconciliationQueue.append({.connectorName = output->connectorName,
                                    .profilePath = profile->sourcePath});
    }
  }
  if (m_reconciliationQueue.isEmpty())
    return;

  m_currentApplyIsReconciliation = true;
  m_pendingColorApplication = m_reconciliationQueue.takeFirst();
  m_operationStatusText = tr("Restoring saved color profiles…");
  applyPendingColorProfile();
}

void ColorSettingsModel::outputManagementOwnerChanged(
    const quint64 ownerGeneration, const bool available) {
  m_colorOwnerGeneration = ownerGeneration;
  m_colorPortAvailable = available;
  if (available)
    m_reconciliationNeeded = true;
  if (!available && m_colorApplyInFlight) {
    m_colorApplyInFlight = false;
    m_pendingColorApplication.reset();
    m_reconciliationQueue.clear();
    m_currentApplyIsReconciliation = false;
    m_operationStatusText.clear();
    m_errorText = tr("The display connection was lost before the color profile "
                     "was confirmed.");
  }
  if (available)
    reconcileSavedProfiles();
  Q_EMIT viewChanged();
}

void ColorSettingsModel::outputManagementCompleted(
    const quint64 ownerGeneration, const quint64 requestId,
    const CompletionOutcome outcome) {
  if (!m_colorApplyInFlight || ownerGeneration != m_colorOwnerGeneration ||
      requestId != m_colorRequestId) {
    return;
  }
  m_colorApplyInFlight = false;
  const bool removing = m_pendingColorApplication.has_value() &&
                        m_pendingColorApplication->profilePath.isEmpty();
  m_pendingColorApplication.reset();
  m_operationStatusText.clear();
  if (outcome == CompletionOutcome::Applied) {
    m_errorText.clear();
    if (m_currentApplyIsReconciliation && !m_reconciliationQueue.isEmpty()) {
      m_pendingColorApplication = m_reconciliationQueue.takeFirst();
      m_operationStatusText = tr("Restoring saved color profiles…");
      applyPendingColorProfile();
      Q_EMIT viewChanged();
      return;
    }
    m_operationStatusText =
        m_currentApplyIsReconciliation ? tr("Saved color profiles are active.")
        : removing ? tr("The standard color profile is active.")
                   : tr("The color profile is active on this display.");
  } else if (outcome == CompletionOutcome::Rejected) {
    m_errorText = tr("The display rejected this color profile.");
  } else {
    m_errorText =
        tr("The color profile could not be confirmed on the display.");
  }
  m_reconciliationQueue.clear();
  m_currentApplyIsReconciliation = false;
  Q_EMIT viewChanged();
}


} // namespace QindaQt::Apps::SettingsColor
