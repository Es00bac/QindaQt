// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/clipboard_service/clipboard_host.h>

#include <qindaqt/services/clipboard_model/clipboard_descriptor.h>

namespace QindaQt::Services::Clipboard {

ClipboardHost::ClipboardHost(ClipboardWayland::ClipboardWaylandAdapter *adapter,
                             quint64 epoch, QObject *parent)
    : QObject(parent), m_adapter(adapter), m_epoch(epoch == 0 ? 1 : epoch)
{
    Q_ASSERT(m_adapter != nullptr);
    m_adapter->setObserver(this);
    m_adapter->setCaptureEnabled(false);
}

ClipboardHost::~ClipboardHost()
{
    m_adapter->setCaptureEnabled(false);
    m_adapter->setObserver(nullptr);
}

Snapshot ClipboardHost::snapshot() const
{
    const ClipboardModel::HistorySnapshot history = m_history.snapshot();
    const auto encoded = ClipboardModel::encodeDescriptorList(history.entries);
    return {.schemaVersion = kSchemaVersion,
            .epoch = m_epoch,
            .generation = history.generation,
            .revision = history.revision,
            .historyEnabled = history.historyEnabled,
            .privacyAllowed = history.privacyAllowed,
            .descriptorList = encoded.accepted() ? encoded.bytes : QByteArray{},
            .wireValid = encoded.accepted()};
}

void ClipboardHost::publishIfChanged(quint32 generation, quint64 revision,
                                     bool enabled, bool allowed)
{
    const auto current = m_history.snapshot();
    if (generation != current.generation || revision != current.revision
        || enabled != current.historyEnabled || allowed != current.privacyAllowed) {
        Q_EMIT changed(m_epoch, current.generation, current.revision);
    }
}

void ClipboardHost::setHistoryOptIn(bool enabled)
{
    const auto before = m_history.snapshot();
    m_optedIn = enabled;
    m_history.setHistoryEnabled(enabled);
    m_adapter->setCaptureEnabled(m_optedIn && m_unlocked && m_adapter->isAvailable());
    publishIfChanged(before.generation, before.revision, before.historyEnabled,
                     before.privacyAllowed);
}

void ClipboardHost::setUnlocked(bool unlocked)
{
    const auto before = m_history.snapshot();
    m_unlocked = unlocked;
    m_history.setPrivacyAllowed(unlocked);
    m_adapter->setCaptureEnabled(m_optedIn && m_unlocked && m_adapter->isAvailable());
    publishIfChanged(before.generation, before.revision, before.historyEnabled,
                     before.privacyAllowed);
}

void ClipboardHost::captureAvailabilityChanged(bool available)
{
    m_adapter->setCaptureEnabled(available && m_optedIn && m_unlocked);
}

void ClipboardHost::captured(ClipboardWayland::SelectionKind kind,
                             const ClipboardModel::ClipboardValue &value)
{
    const auto before = m_history.snapshot();
    const QString label = kind == ClipboardWayland::SelectionKind::Clipboard
        ? QStringLiteral("Wayland clipboard") : QStringLiteral("Wayland primary");
    const auto admitted = m_history.admit(value, before.generation, label, ++m_tick);
    if (admitted.accepted()) {
        const auto after = m_history.snapshot();
        Q_EMIT changed(m_epoch, after.generation, after.revision);
    }
}

void ClipboardHost::captureRefused(ClipboardWayland::SelectionKind,
                                   ClipboardModel::ClipboardError)
{
    // Intentionally silent: refusal reason is testable policy state, while
    // producer MIME/payload details must never become diagnostics.
}

OperationResult ClipboardHost::resultFor(const OperationRequest &request,
                                         OperationStatus status,
                                         const QString &reasonCode) const
{
    const auto current = m_history.snapshot();
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
    if (request.expectedEpoch != m_epoch) {
        return resultFor(request, OperationStatus::Rejected, QStringLiteral("stale-epoch"));
    }
    const auto before = m_history.snapshot();
    if (request.expectedGeneration != before.generation
        || request.expectedRevision != before.revision) {
        return resultFor(request, OperationStatus::Rejected, QStringLiteral("stale-lineage"));
    }

    ClipboardModel::ClipboardError error = ClipboardModel::ClipboardError::None;
    bool uncertain = false;
    switch (request.kind) {
    case OperationKind::Select: {
        const auto outcome = m_history.promote(request.entry, request.expectedGeneration, ++m_tick);
        error = outcome.error;
        break;
    }
    case OperationKind::Delete:
        error = m_history.removeEntry(request.entry, request.expectedGeneration).error;
        break;
    case OperationKind::Clear:
        error = m_history.clear(request.clearAll ? ClipboardModel::ClearScope::All
                                                : ClipboardModel::ClearScope::UnpinnedOnly,
                                request.expectedGeneration).error;
        break;
    case OperationKind::Copy: {
        if (!m_adapter->isAvailable()) {
            return resultFor(request, OperationStatus::Failed,
                             QStringLiteral("wayland-unavailable"));
        }
        const auto outcome = m_history.promote(request.entry, request.expectedGeneration, ++m_tick);
        error = outcome.error;
        if (outcome.accepted()) {
            uncertain = !m_adapter->publishSelection(
                ClipboardWayland::SelectionKind::Clipboard, outcome.value);
        }
        break;
    }
    }
    if (error != ClipboardModel::ClipboardError::None) {
        return resultFor(request, OperationStatus::Rejected, reasonFor(error));
    }
    const auto after = m_history.snapshot();
    if (after.revision != before.revision) {
        Q_EMIT changed(m_epoch, after.generation, after.revision);
    }
    return resultFor(request, uncertain ? OperationStatus::Uncertain
                                        : OperationStatus::Succeeded,
                     uncertain ? QStringLiteral("wayland-uncertain")
                               : QStringLiteral("ok"));
}

} // namespace QindaQt::Services::Clipboard
