// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/clipboard_service/clipboard_host.h>
#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>

#include "clipboard_privacy_state_p.h"

#include <utility>

namespace QindaQt::Services::Clipboard {

ClipboardHost::ClipboardHost(ClipboardWayland::ClipboardWaylandAdapter *adapter,
                             quint64 epoch, QObject *parent)
    : ClipboardHost(adapter, epoch, [] { return true; }, parent)
{
}

ClipboardHost::ClipboardHost(ClipboardWayland::ClipboardWaylandAdapter *adapter,
                             quint64 epoch, PrivacyAdmission admission, QObject *parent)
    : QObject(parent), m_adapter(adapter), m_epoch(epoch == 0 ? 1 : epoch)
{
    Q_ASSERT(m_adapter != nullptr);
    m_privacy = std::make_unique<ClipboardPrivacyState>(*m_adapter, std::move(admission),
        [this](const ClipboardModel::HistorySnapshot &before) { publishIfChanged(before); });
    m_adapter->setObserver(this);
    m_adapter->setCaptureEnabled(false);
}

ClipboardHost::~ClipboardHost()
{
    m_adapter->setObserver(nullptr);
    m_adapter->setCaptureEnabled(false);
}

Snapshot ClipboardHost::snapshot() const
{
    (void)m_privacy->check();
    auto history = m_privacy->history.snapshot();
    auto encoded = ClipboardModel::encodeDescriptorList(history.entries);
    // Admission may deny reentrantly. Never return the descriptor copy encoded
    // before that reconciliation, even when flags alone changed.
    (void)m_privacy->check();
    const auto current = m_privacy->history.snapshot();
    if (current != history) {
        history = current;
        encoded = ClipboardModel::encodeDescriptorList(history.entries);
    }
    return {.schemaVersion = kSchemaVersion,
            .epoch = m_epoch,
            .generation = history.generation,
            .revision = history.revision,
            .historyEnabled = history.historyEnabled,
            .privacyAllowed = history.privacyAllowed,
            .descriptorList = encoded.accepted() ? encoded.bytes : QByteArray{},
            .wireValid = encoded.accepted()};
}

void ClipboardHost::publishIfChanged(const ClipboardModel::HistorySnapshot &before)
{
    const auto current = m_privacy->history.snapshot();
    if (before != current) Q_EMIT changed(m_epoch, current.generation, current.revision);
}

void ClipboardHost::setHistoryOptIn(bool enabled) { m_privacy->setHistoryOptIn(enabled); }
void ClipboardHost::setUnlocked(bool unlocked) { m_privacy->setUnlocked(unlocked); }
void ClipboardHost::captureAvailabilityChanged(bool available)
{
    m_privacy->synchronizeCapture(available);
}

bool ClipboardHost::operationAdmitted(quint32 generation)
{
    return m_privacy->check() && m_privacy->history.isHistoryEnabled()
        && m_privacy->history.generation() == generation;
}

void ClipboardHost::captured(ClipboardWayland::SelectionKind kind,
                             const ClipboardModel::ClipboardValue &value)
{
    // AGENT-GUARD: denied admission cancels the adapter transfer synchronously.
    // value may alias that destroyed transfer; do not touch it after denial.
    if (!m_privacy->check() || !m_privacy->history.isHistoryEnabled()) return;
    const auto before = m_privacy->history.snapshot();
    const QString label = kind == ClipboardWayland::SelectionKind::Clipboard
        ? QStringLiteral("Wayland clipboard") : QStringLiteral("Wayland primary");
    const auto admitted = m_privacy->history.admit(value, before.generation, label, ++m_tick);
    if (!admitted.accepted() || !operationAdmitted(before.generation)) return;
    const auto after = m_privacy->history.snapshot();
    Q_EMIT changed(m_epoch, after.generation, after.revision);
    (void)m_privacy->check();
}

void ClipboardHost::captureRefused(ClipboardWayland::SelectionKind,
                                   ClipboardModel::ClipboardError)
{
    // Intentionally silent: producer MIME/payload details never become diagnostics.
}

OperationResult ClipboardHost::resultFor(const OperationRequest &request,
                                         OperationStatus status,
                                         const QString &reasonCode) const
{
    const auto current = m_privacy->history.snapshot();
    return {.kind = request.kind,
            .status = status,
            .requestId = request.requestId,
            .initiatingEpoch = request.expectedEpoch,
            .initiatingGeneration = request.expectedGeneration,
            .initiatingRevision = request.expectedRevision,
            .observedEpoch = m_epoch,
            .observedGeneration = current.generation,
            .observedRevision = current.revision,
            .reasonCode = reasonCode};
}

QString ClipboardHost::reasonFor(ClipboardModel::ClipboardError error)
{
    switch (error) {
    case ClipboardModel::ClipboardError::None: return QStringLiteral("ok");
    case ClipboardModel::ClipboardError::HistoryDisabled: return QStringLiteral("history-disabled");
    case ClipboardModel::ClipboardError::PrivacyDenied: return QStringLiteral("privacy-denied");
    case ClipboardModel::ClipboardError::StaleGeneration: return QStringLiteral("stale-generation");
    case ClipboardModel::ClipboardError::LineageExhausted: return QStringLiteral("lineage-exhausted");
    case ClipboardModel::ClipboardError::UnknownEntry: return QStringLiteral("unknown-entry");
    case ClipboardModel::ClipboardError::PinnedLimitReached: return QStringLiteral("pin-limit");
    case ClipboardModel::ClipboardError::CapacityRefused: return QStringLiteral("capacity-refused");
    default: return QStringLiteral("value-refused");
    }
}

OperationResult ClipboardHost::submit(const OperationRequest &request)
{
    (void)m_privacy->check();
    if (request.expectedEpoch != m_epoch)
        return resultFor(request, OperationStatus::Rejected, QStringLiteral("stale-epoch"));
    const auto before = m_privacy->history.snapshot();
    if (request.expectedGeneration != before.generation
        || request.expectedRevision != before.revision)
        return resultFor(request, OperationStatus::Rejected, QStringLiteral("stale-lineage"));

    auto &history = m_privacy->history;
    ClipboardModel::ClipboardError error = ClipboardModel::ClipboardError::None;
    bool uncertain = false;
    bool sent = false;
    switch (request.kind) {
    case OperationKind::Select:
        error = history.promote(request.entry, request.expectedGeneration, ++m_tick).error;
        break;
    case OperationKind::Delete:
        error = history.removeEntry(request.entry, request.expectedGeneration).error;
        break;
    case OperationKind::Clear:
        error = history.clear(request.clearAll ? ClipboardModel::ClearScope::All
                                              : ClipboardModel::ClearScope::UnpinnedOnly,
                              request.expectedGeneration).error;
        break;
    case OperationKind::Copy: {
        const bool available = m_adapter->isAvailable();
        if (!operationAdmitted(before.generation))
            return resultFor(request, OperationStatus::Rejected, QStringLiteral("privacy-changed"));
        if (!available)
            return resultFor(request, OperationStatus::Failed, QStringLiteral("wayland-unavailable"));
        // promote returns an owned payload copy. No reference into history
        // survives the external admission/publication calls.
        const auto outcome = history.promote(request.entry, request.expectedGeneration, ++m_tick);
        error = outcome.error;
        if (outcome.accepted()) {
            if (!operationAdmitted(before.generation))
                return resultFor(request, OperationStatus::Rejected, QStringLiteral("privacy-changed"));
            sent = true;
            uncertain = !m_adapter->publishSelection(
                ClipboardWayland::SelectionKind::Clipboard, outcome.value);
            if (!operationAdmitted(before.generation))
                return resultFor(request, OperationStatus::Uncertain, QStringLiteral("privacy-changed"));
        }
        break;
    }
    }
    if (error != ClipboardModel::ClipboardError::None)
        return resultFor(request, OperationStatus::Rejected, reasonFor(error));
    if (!operationAdmitted(before.generation))
        return resultFor(request, OperationStatus::Uncertain, QStringLiteral("privacy-changed"));
    publishIfChanged(before);
    if (!operationAdmitted(before.generation))
        return resultFor(request, OperationStatus::Uncertain, QStringLiteral("privacy-changed"));
    return resultFor(request, uncertain && sent ? OperationStatus::Uncertain
                                                : OperationStatus::Succeeded,
                     uncertain ? QStringLiteral("wayland-uncertain") : QStringLiteral("ok"));
}

} // namespace QindaQt::Services::Clipboard
