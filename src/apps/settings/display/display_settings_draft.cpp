// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_display/display_settings_model.h>
#include <qindaqt/apps/settings_display/display_settings_values.h>

#include <QtCore/QCoreApplication>
#include <algorithm>

namespace QindaQt::Apps::SettingsDisplay {

bool DisplaySettingsModel::setOutputEnabled(const QString &stableId, bool enabled) {
  if (!canEdit()) {
    return false;
  }
  auto *out = findDraftOutput(stableId);
  if (out == nullptr || out->enabled == enabled) {
    return false;
  }

  out->enabled = enabled;
  if (!enabled && out->primary) {
    out->primary = false;
    for (auto &other : m_draftOutputs) {
      if (other.stableId != stableId && other.enabled) {
        other.primary = true;
        break;
      }
    }
  } else if (enabled) {
    bool hasPrimary = false;
    for (const auto &other : m_draftOutputs) {
      if (other.enabled && other.primary) {
        hasPrimary = true;
        break;
      }
    }
    if (!hasPrimary) {
      out->primary = true;
    }
  }

  validateDraft();
  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT stateChanged();
  return true;
}

bool DisplaySettingsModel::setOutputPrimary(const QString &stableId) {
  if (!canEdit()) {
    return false;
  }
  auto *out = findDraftOutput(stableId);
  if (out == nullptr) {
    return false;
  }

  for (auto &other : m_draftOutputs) {
    if (other.stableId == stableId) {
      other.enabled = true;
      other.primary = true;
    } else {
      other.primary = false;
    }
  }

  validateDraft();
  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT stateChanged();
  return true;
}

bool DisplaySettingsModel::setOutputMode(const QString &stableId,
                                         const QString &modeId) {
  if (!canEdit()) {
    return false;
  }
  auto *out = findDraftOutput(stableId);
  if (out == nullptr || out->modeId == modeId) {
    return false;
  }

  bool modeFound = false;
  for (const auto &m : out->modes) {
    if (m.id == modeId) {
      modeFound = true;
      break;
    }
  }
  if (!modeFound) {
    return false;
  }

  out->modeId = modeId;
  validateDraft();
  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT stateChanged();
  return true;
}

bool DisplaySettingsModel::setOutputScale(const QString &stableId,
                                         double scale) {
  if (!canEdit()) {
    return false;
  }
  auto *out = findDraftOutput(stableId);
  if (out == nullptr) {
    return false;
  }
  if (qFuzzyCompare(out->scale, scale)) {
    return false;
  }

  out->scale = scale;
  validateDraft();
  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT stateChanged();
  return true;
}

bool DisplaySettingsModel::setOutputTransform(const QString &stableId,
                                              const QString &transformStr) {
  if (!canEdit()) {
    return false;
  }
  auto *out = findDraftOutput(stableId);
  if (out == nullptr) {
    return false;
  }

  const auto transform = parseTransform(transformStr);
  if (out->transform == transform) {
    return false;
  }

  out->transform = transform;
  validateDraft();
  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT stateChanged();
  return true;
}

bool DisplaySettingsModel::setOutputPosition(const QString &stableId, int x,
                                             int y) {
  if (!canEdit()) {
    return false;
  }
  auto *out = findDraftOutput(stableId);
  if (out == nullptr) {
    return false;
  }

  const QPoint newPos(x, y);
  if (out->position == newPos) {
    return false;
  }

  out->position = newPos;
  validateDraft();
  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT stateChanged();
  return true;
}

bool DisplaySettingsModel::cancelDraft() {
  if (!canEdit() || !m_draftDirty) {
    return false;
  }
  resetDraftToSnapshot();
  m_transientError.clear();
  Q_EMIT outputsChanged();
  Q_EMIT selectedOutputChanged();
  Q_EMIT draftChanged();
  Q_EMIT stateChanged();
  return true;
}

void DisplaySettingsModel::resetDraftToSnapshot() {
  m_draftOutputs.clear();
  m_draftDirty = false;
  m_draftValid = true;
  m_draftErrorMessage.clear();
  m_fieldErrors.clear();
  m_warnings.clear();

  if (!m_snapshot.has_value()) {
    m_selectedOutputId.clear();
    return;
  }

  QString firstPrimaryId;
  QString firstId;

  for (const auto &out : m_snapshot->outputs) {
    OutputDraft draft;
    draft.stableId = out.stableId;
    draft.connectorName = out.connectorName;
    draft.label = out.label;
    draft.manufacturer = out.manufacturer;
    draft.model = out.model;
    draft.physicalSizeMillimeters = out.physicalSizeMillimeters;
    draft.internal = out.internal;
    draft.enabled = out.enabled;
    draft.primary = out.primary;
    draft.modeId = out.modeId;
    draft.position = out.position;
    draft.logicalSize = out.logicalSize;
    draft.scale = out.scale;
    draft.transform = out.transform;
    draft.priority = out.priority;
    draft.replicationSourceStableId = out.replicationSourceStableId;
    draft.modes = formatModes(out.modes);
    m_draftOutputs.append(draft);

    if (firstId.isEmpty()) {
      firstId = out.stableId;
    }
    if (out.primary && firstPrimaryId.isEmpty()) {
      firstPrimaryId = out.stableId;
    }
  }

  if (findDraftOutput(m_selectedOutputId) == nullptr) {
    m_selectedOutputId = !firstPrimaryId.isEmpty() ? firstPrimaryId : firstId;
    Q_EMIT selectedOutputIdChanged(m_selectedOutputId);
  }
}

void DisplaySettingsModel::validateDraft() {
  if (!m_snapshot.has_value()) {
    m_draftValid = false;
    m_draftErrorMessage = QCoreApplication::translate(
        "DisplaySettings", "No active display snapshot.");
    m_fieldErrors.clear();
    m_warnings.clear();
    m_draftDirty = false;
    return;
  }

  const Display::Candidate candidate = buildCandidateFromDraft();
  const auto valResult =
      DisplayTopology::validateAndNormalize(*m_snapshot, candidate);

  m_draftValid = valResult.accepted();
  m_draftDirty = !valResult.noOp;

  m_warnings.clear();
  for (const auto &w : valResult.warnings) {
    if (w.kind == DisplayTopology::TopologyWarningKind::DisconnectedGap) {
      m_warnings.append(QCoreApplication::translate(
          "DisplaySettings", "Display topology contains gaps between monitors."));
    } else if (w.kind ==
               DisplayTopology::TopologyWarningKind::NonIntegralLogicalExtent) {
      m_warnings.append(QCoreApplication::translate(
          "DisplaySettings",
          "Fractional scale results in non-integral logical dimensions."));
    }
  }

  m_fieldErrors.clear();
  if (!valResult.accepted()) {
    m_draftErrorMessage =
        formatTopologyError(valResult.error, valResult.reasonCode);
    if (!valResult.offendingStableId.isEmpty()) {
      m_fieldErrors[valResult.offendingStableId] = m_draftErrorMessage;
    }
  } else {
    m_draftErrorMessage.clear();
  }

  // Update logical sizes in draft from geometries
  for (const auto &geom : valResult.geometries) {
    auto *out = findDraftOutput(geom.stableId);
    if (out != nullptr) {
      out->logicalSize = geom.logicalRect.size();
    }
  }
}

Display::Candidate DisplaySettingsModel::buildCandidateFromDraft() const {
  Display::Candidate candidate;
  candidate.protocolVersion = 1;
  if (m_snapshot.has_value()) {
    candidate.baseEpoch = m_snapshot->serviceEpoch;
    candidate.baseRevision = m_snapshot->revision;
  }
  candidate.outputs.reserve(m_draftOutputs.size());
  for (const auto &draft : m_draftOutputs) {
    Display::CandidateOutput out;
    out.stableId = draft.stableId;
    out.enabled = draft.enabled;
    out.primary = draft.primary;
    out.modeId = draft.modeId;
    out.position = draft.position;
    out.scale = draft.scale;
    out.transform = draft.transform;
    out.priority = draft.priority;
    out.replicationSourceStableId = draft.replicationSourceStableId;
    candidate.outputs.append(out);
  }
  return candidate;
}

OutputDraft *DisplaySettingsModel::findDraftOutput(const QString &stableId) {
  for (auto &out : m_draftOutputs) {
    if (out.stableId == stableId) {
      return &out;
    }
  }
  return nullptr;
}

const OutputDraft *
DisplaySettingsModel::findDraftOutput(const QString &stableId) const {
  for (const auto &out : m_draftOutputs) {
    if (out.stableId == stableId) {
      return &out;
    }
  }
  return nullptr;
}

} // namespace QindaQt::Apps::SettingsDisplay
