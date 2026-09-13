// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_power/external_display_brightness_model.h>

#include <qindaqt/services/brightness_model/brightness_math.h>
#include <qindaqt/services/display_protocol/display_limits.h>
#include <qindaqt/services/display_protocol/display_validation.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariantMap>

#include <utility>

namespace QindaQt::Apps::SettingsPower {
namespace {

using DisplayClient::ClientState;
using Display::OperationStatus;

QString translated(const char *text) {
  return QCoreApplication::translate("PowerSettings", text);
}

struct Entry {
  QString rowId;
  const Display::Output *output = nullptr;
  const Display::OutputBrightness *level = nullptr;
};

// Rows follow the joined snapshot order. Internal panels belong to Power1 and
// disabled outputs are not presented. Entries borrow the passed values.
QList<Entry> presentedOutputs(const Display::Snapshot &snapshot,
                              const Display::BrightnessSnapshot &brightness) {
  QList<Entry> entries;
  if (!Display::validateBrightnessJoin(snapshot, brightness).accepted) return entries;
  for (qsizetype index = 0; index < snapshot.outputs.size(); ++index) {
    const Display::Output &output = snapshot.outputs.at(index);
    if (!output.enabled || output.internal) continue;
    entries.append({QStringLiteral("external-%1").arg(entries.size() + 1), &output,
                    &brightness.outputs.at(index)});
  }
  return entries;
}

QString displayName(const Display::Output &output) {
  if (!output.label.trimmed().isEmpty()) return output.label.trimmed();
  QStringList parts;
  if (!output.manufacturer.trimmed().isEmpty()) parts.append(output.manufacturer.trimmed());
  if (!output.model.trimmed().isEmpty()) parts.append(output.model.trimmed());
  return parts.isEmpty() ? output.connectorName : parts.join(QLatin1Char(' '));
}

// The visible explanation for an output Display1 would refuse (ADR-0150).
QString refusal(const Display::Output &output, const Display::OutputBrightness &level) {
  if (!output.replicationSourceStableId.isEmpty())
    return translated("This display mirrors another display, so its brightness is not adjusted here");
  if (output.ambiguousIdentity)
    return translated("This display cannot be identified uniquely, so its brightness is not adjusted");
  if (!level.capable) return translated("This display does not offer brightness control");
  if (!level.observed) return translated("Brightness is not reported for this display");
  return {};
}

} // namespace

ExternalDisplayBrightnessModel::ExternalDisplayBrightnessModel(
    DisplayClient::Client &client, QObject *parent)
    : QObject(parent), m_client(client) {
  m_debounceTimer.setSingleShot(true);
  m_debounceTimer.setInterval(120);
  m_convergenceTimer.setSingleShot(true);
  m_convergenceTimer.setInterval(5000);
  connect(&m_debounceTimer, &QTimer::timeout, this,
          &ExternalDisplayBrightnessModel::dispatchDebounced);
  connect(&m_convergenceTimer, &QTimer::timeout, this, [this] {
    if (!m_convergence) return;
    m_convergence.reset();
    m_operationStatusText.clear();
    m_errorText = tr("The display accepted the change but did not report its brightness in time. It was not replayed.");
    Q_EMIT viewChanged();
  });
  connect(&m_client, &DisplayClient::Client::stateChanged, this,
          [this] { synchronizeAuthority(); });
  connect(&m_client, &DisplayClient::Client::snapshotChanged, this,
          [this] { synchronizeAuthority(); });
  connect(&m_client, &DisplayClient::Client::brightnessChanged, this,
          [this] { synchronizeAuthority(); });
  connect(&m_client, &DisplayClient::Client::operationCompleted, this,
          &ExternalDisplayBrightnessModel::handleOperationCompleted);
}

QVariantList ExternalDisplayBrightnessModel::rows() const {
  QVariantList result;
  const auto snapshot = m_client.snapshot();
  const auto brightness = m_client.brightness();
  if (!snapshot || !brightness) return result;
  for (const Entry &entry : presentedOutputs(*snapshot, *brightness)) {
    const QString name = displayName(*entry.output);
    const QString reason = refusal(*entry.output, *entry.level);
    const auto normalized = entry.level->observed
        ? Brightness::normalizeRaw(0, Display::kMaxBrightness, entry.level->value)
        : Brightness::NormalizedResult{};
    // A level reported for an output KWin cannot adjust is not shown as one.
    const bool known = entry.level->capable && entry.level->observed
        && normalized.succeeded();
    const bool settable = known && reason.isEmpty();
    QString description;
    if (!known)
      description = tr("%1, brightness unavailable, read-only").arg(name);
    else if (settable)
      description = tr("%1, normalized %2 of 10000").arg(name).arg(normalized.value);
    else
      description = tr("%1, normalized %2 of 10000, read-only").arg(name).arg(normalized.value);
    result.append(QVariantMap{
        {QStringLiteral("id"), entry.rowId},
        {QStringLiteral("name"), name},
        {QStringLiteral("known"), known},
        {QStringLiteral("normalized"), known ? normalized.value : 0U},
        {QStringLiteral("settable"), settable},
        {QStringLiteral("available"), settable && admission(entry.rowId, true).isEmpty()},
        {QStringLiteral("reason"), reason},
        {QStringLiteral("accessibleDescription"), description},
    });
  }
  return result;
}

bool ExternalDisplayBrightnessModel::busy() const noexcept {
  return m_debounce.has_value() || m_pending.has_value()
      || m_convergence.has_value() || m_client.operationPending();
}

std::optional<ExternalDisplayBrightnessModel::Lineage>
ExternalDisplayBrightnessModel::currentLineage() const {
  const auto brightness = m_client.brightness();
  if (m_client.owner().isEmpty() || !brightness) return std::nullopt;
  return Lineage{m_client.owner(), brightness->serviceEpoch, brightness->revision};
}

std::optional<ExternalDisplayBrightnessModel::Target>
ExternalDisplayBrightnessModel::findTarget(const QString &rowId) const {
  const auto snapshot = m_client.snapshot();
  const auto brightness = m_client.brightness();
  if (!snapshot || !brightness) return std::nullopt;
  for (const Entry &entry : presentedOutputs(*snapshot, *brightness)) {
    if (entry.rowId != rowId) continue;
    const bool settable = entry.level->capable && entry.level->observed
        && refusal(*entry.output, *entry.level).isEmpty();
    return Target{entry.output->stableId, settable, entry.level->value};
  }
  return std::nullopt;
}

QString ExternalDisplayBrightnessModel::admission(const QString &rowId,
                                                  const bool replacingDebounce) const {
  if (m_pending || m_convergence || m_client.operationPending())
    return QStringLiteral("operation-busy");
  if (m_debounce && (!replacingDebounce || m_debounce->rowId != rowId))
    return QStringLiteral("operation-busy");
  if (m_client.state() != ClientState::Ready || !currentLineage())
    return QStringLiteral("unavailable");
  // AGENT-NOTE: Display1 admits immediate brightness only while no
  // arrangement transaction is live; offering the control meanwhile would
  // only produce a refusal.
  if (const auto snapshot = m_client.snapshot(); !snapshot || !snapshot->transactions.isEmpty())
    return QStringLiteral("display-busy");
  const auto target = findTarget(rowId);
  if (!target) return QStringLiteral("stale-handle");
  if (!target->settable) return QStringLiteral("unsupported");
  return {};
}

bool ExternalDisplayBrightnessModel::requestBrightness(const QString &rowId,
                                                       const int normalized) {
  const QString reason = admission(rowId, true);
  if (!reason.isEmpty() || normalized < 0
      || normalized > static_cast<int>(Display::kMaxBrightness)) {
    reject(reason.isEmpty() ? QStringLiteral("invalid-brightness") : reason);
    return false;
  }
  const auto target = findTarget(rowId);
  const auto lineage = currentLineage();
  const auto value = Brightness::denormalizeRaw(0, Display::kMaxBrightness,
                                                static_cast<quint32>(normalized));
  if (!target || !lineage || !value.succeeded()) {
    reject(QStringLiteral("stale-handle"));
    return false;
  }
  // AGENT-GUARD: returning a gesture to observed truth cancels its queued
  // predecessor instead of sending an idempotent request.
  if (value.value == target->value) {
    m_debounceTimer.stop();
    m_debounce.reset();
    m_errorText.clear();
    m_operationStatusText.clear();
    Q_EMIT viewChanged();
    return true;
  }
  m_debounce = Debounced{rowId, target->stableId, value.value, *lineage};
  m_debounceTimer.start();
  m_errorText.clear();
  m_operationStatusText = tr("Brightness change queued…");
  Q_EMIT viewChanged();
  return true;
}

void ExternalDisplayBrightnessModel::dispatchDebounced() {
  if (!m_debounce) return;
  const Debounced debounce = *std::exchange(m_debounce, std::nullopt);
  const auto lineage = currentLineage();
  if (!lineage || lineage->owner != debounce.lineage.owner
      || lineage->epoch != debounce.lineage.epoch) {
    reject(QStringLiteral("authority-changed"));
    return;
  }
  if (lineage->revision != debounce.lineage.revision) {
    reject(QStringLiteral("stale-handle"));
    return;
  }
  // Final dispatch re-runs the same predicate that enabled the control.
  const QString reason = admission(debounce.rowId, false);
  if (!reason.isEmpty()) {
    reject(reason);
    return;
  }
  const auto target = findTarget(debounce.rowId);
  if (!target || target->stableId != debounce.stableId) {
    reject(QStringLiteral("stale-handle"));
    return;
  }
  if (target->value == debounce.value) {
    m_operationStatusText.clear();
    Q_EMIT viewChanged();
    return;
  }
  const quint64 requestId = m_client.setOutputBrightness(
      {.baseEpoch = lineage->epoch,
       .baseRevision = lineage->revision,
       .stableId = target->stableId,
       .value = debounce.value});
  if (requestId == 0) {
    reject(QStringLiteral("request-id-exhausted"));
    return;
  }
  m_pending = Pending{requestId, target->stableId, *lineage, debounce.value};
  m_operationStatusText = tr("Applying display brightness…");
  Q_EMIT viewChanged();
}

void ExternalDisplayBrightnessModel::handleOperationCompleted(
    const quint64 requestId, const Display::OperationResult &result) {
  if (!m_pending || m_pending->requestId != requestId) return;
  const Pending pending = *std::exchange(m_pending, std::nullopt);
  const bool exact = m_client.owner() == pending.lineage.owner && result.wireValid
      && Display::validateOperationResult(result).accepted
      && result.kind == Display::OperationKind::ImmediatePolicy
      && result.initiatingEpoch == pending.lineage.epoch;
  m_operationStatusText.clear();
  if (exact && result.status == OperationStatus::Succeeded
      && result.initiatingRevision == pending.lineage.revision) {
    m_convergence = Convergence{pending.stableId, pending.lineage.owner,
                                pending.lineage.epoch, result.observedRevision,
                                pending.expected};
    m_errorText.clear();
    m_operationStatusText = tr("Waiting for the display to report its brightness…");
    m_convergenceTimer.start();
  } else if (exact && result.status == OperationStatus::Busy) {
    m_errorText = tr("The display service was busy and did not change the brightness. Nothing was replayed.");
  } else if (exact && result.status == OperationStatus::Rejected) {
    m_errorText = failureText(result.diagnostic);
  } else {
    m_errorText = tr("The display brightness change could not be confirmed. It was not replayed.");
  }
  synchronizeAuthority();
}

void ExternalDisplayBrightnessModel::settleConvergence() {
  const auto brightness = m_client.brightness();
  if (m_client.owner() != m_convergence->owner
      || (brightness && brightness->serviceEpoch != m_convergence->epoch)) {
    m_convergence.reset();
    m_errorText = tr("Display authority changed before the brightness change converged. It was not replayed.");
  } else if (const auto snapshot = m_client.snapshot();
             snapshot && brightness
             && brightness->revision >= m_convergence->minimumRevision) {
    // AGENT-NOTE: Display1 replies only after it observes a republication
    // (ADR-0150), so joined rows at the observed revision are the answer: a
    // different level settled elsewhere and is shown as is.
    std::optional<quint32> observed;
    for (const Entry &entry : presentedOutputs(*snapshot, *brightness))
      if (entry.output->stableId == m_convergence->stableId && entry.level->observed)
        observed = entry.level->value;
    if (!observed)
      m_errorText = tr("The display changed before its brightness could be confirmed. It was not replayed.");
    else if (*observed != m_convergence->expected)
      m_errorText = tr("The display settled at a different brightness than requested. The observed level is shown and nothing was replayed.");
    m_convergence.reset();
  }
  if (!m_convergence) {
    m_convergenceTimer.stop();
    m_operationStatusText.clear();
  }
}

void ExternalDisplayBrightnessModel::synchronizeAuthority() {
  const auto lineage = currentLineage();
  if (m_debounce && (!lineage || lineage->owner != m_debounce->lineage.owner
                     || lineage->epoch != m_debounce->lineage.epoch)) {
    m_debounceTimer.stop();
    m_debounce.reset();
    m_operationStatusText.clear();
    m_errorText = tr("Display authority changed before the queued brightness request; no change was sent.");
  }
  if (m_pending && m_client.owner() != m_pending->lineage.owner) {
    m_pending.reset();
    m_operationStatusText.clear();
    m_errorText = tr("Display authority changed during the brightness change. It was not replayed.");
  }
  if (m_convergence) settleConvergence();
  Q_EMIT viewChanged();
}

void ExternalDisplayBrightnessModel::reject(const QString &reason) {
  m_operationStatusText.clear();
  m_errorText = failureText(reason);
  Q_EMIT actionRejected(reason);
  Q_EMIT viewChanged();
}

QString ExternalDisplayBrightnessModel::failureText(const QString &reason) const {
  if (reason == QStringLiteral("operation-busy"))
    return tr("Another display brightness change is still in progress.");
  if (reason == QStringLiteral("display-busy"))
    return tr("A display arrangement change is in progress. Try again when it finishes.");
  if (reason == QStringLiteral("unavailable"))
    return tr("Display brightness is unavailable right now.");
  if (reason == QStringLiteral("unsupported")
      || reason.startsWith(QStringLiteral("brightness-un"))
      || reason == QStringLiteral("compositor-unsupported"))
    return tr("That display does not accept brightness changes.");
  if (reason == QStringLiteral("stale-handle") || reason == QStringLiteral("authority-changed"))
    return tr("Display state changed before the request could be sent.");
  if (reason == QStringLiteral("stale-revision") || reason == QStringLiteral("unknown-output"))
    return tr("Display state changed before the request was applied. Nothing was replayed.");
  if (reason == QStringLiteral("invalid-brightness"))
    return tr("That brightness value was not accepted.");
  return reason.isEmpty() ? tr("The display brightness request was rejected.")
                          : tr("The display brightness request failed (%1).").arg(reason);
}

} // namespace QindaQt::Apps::SettingsPower
