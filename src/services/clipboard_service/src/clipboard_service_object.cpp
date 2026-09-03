// SPDX-License-Identifier: GPL-3.0-or-later

#include "clipboard_service_object_p.h"

#include <qindaqt/services/clipboard_protocol/clipboard_validation.h>

#include <QtDBus/QDBusMessage>

namespace QindaQt::Services::Clipboard {

ClipboardServiceObject::ClipboardServiceObject(ClipboardHost *host, QObject *parent)
    : QObject(parent), m_host(host)
{
    Q_ASSERT(m_host != nullptr);
    connect(m_host, &ClipboardHost::changed, this, &ClipboardServiceObject::Changed);
}

Snapshot ClipboardServiceObject::GetSnapshot() const { return m_host->snapshot(); }

OperationResult ClipboardServiceObject::Select(quint64 requestId, quint64 epoch,
                                                quint32 generation, quint64 revision,
                                                const ClipboardModel::EntryId &entry)
{
    return submit({OperationKind::Select, requestId, epoch, generation, revision, entry, false});
}

OperationResult ClipboardServiceObject::Delete(quint64 requestId, quint64 epoch,
                                                quint32 generation, quint64 revision,
                                                const ClipboardModel::EntryId &entry)
{
    return submit({OperationKind::Delete, requestId, epoch, generation, revision, entry, false});
}

OperationResult ClipboardServiceObject::Clear(quint64 requestId, quint64 epoch,
                                               quint32 generation, quint64 revision, bool all)
{
    return submit({OperationKind::Clear, requestId, epoch, generation, revision, {}, all});
}

OperationResult ClipboardServiceObject::Copy(quint64 requestId, quint64 epoch,
                                              quint32 generation, quint64 revision,
                                              const ClipboardModel::EntryId &entry)
{
    return submit({OperationKind::Copy, requestId, epoch, generation, revision, entry, false});
}

OperationResult ClipboardServiceObject::submit(OperationRequest request)
{
    const QString caller = calledFromDBus() ? message().service() : QStringLiteral(":local");
    const ValidationResult validation = validateOperationRequest(request);
    if (!validation.accepted) {
        const Snapshot current = m_host->snapshot();
        return {.kind = request.kind, .status = OperationStatus::Rejected,
                .requestId = request.requestId == 0 ? 1 : request.requestId,
                .initiatingEpoch = request.expectedEpoch == 0 ? current.epoch : request.expectedEpoch,
                .initiatingGeneration = request.expectedGeneration == 0 ? current.generation : request.expectedGeneration,
                .initiatingRevision = request.expectedRevision,
                .observedEpoch = current.epoch, .observedGeneration = current.generation,
                .observedRevision = current.revision, .reasonCode = validation.reasonCode};
    }
    auto &requests = m_remembered[caller];
    const auto found = requests.constFind(request.requestId);
    if (found != requests.cend()) {
        if (found->request == request) {
            return found->result;
        }
        OperationResult conflict = found->result;
        conflict.status = OperationStatus::Rejected;
        conflict.reasonCode = QStringLiteral("request-id-conflict");
        return conflict;
    }
    if (requests.size() >= kMaxRememberedRequestsPerCaller
        || (m_remembered.size() > kMaxRememberedCallers && requests.isEmpty())) {
        const Snapshot current = m_host->snapshot();
        return {.kind = request.kind, .status = OperationStatus::Busy,
                .requestId = request.requestId, .initiatingEpoch = request.expectedEpoch,
                .initiatingGeneration = request.expectedGeneration,
                .initiatingRevision = request.expectedRevision,
                .observedEpoch = current.epoch, .observedGeneration = current.generation,
                .observedRevision = current.revision,
                .reasonCode = QStringLiteral("request-cache-full")};
    }
    const OperationResult result = m_host->submit(request);
    requests.insert(request.requestId, {.request = request, .result = result});
    return result;
}

void ClipboardServiceObject::forgetCaller(const QString &caller)
{
    m_remembered.remove(caller);
}

} // namespace QindaQt::Services::Clipboard
