// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/removable_media_client/media_client.h>
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QUuid>

namespace QindaQt::RemovableMedia {
namespace {
ActionAvailability available(const VolumeRow &row, Action action)
{
    switch (action) {
    case Action::Mount: return row.actions.mount;
    case Action::MountReadOnly: return row.actions.mountReadOnly;
    case Action::Unmount: return row.actions.unmount;
    case Action::Remove: return row.actions.remove;
    case Action::ShowDetails: return row.actions.showDetails;
    }
    return {};
}
}
QString MediaClient::requestAction(const Attachment &attachment, Action action)
{
    if (m_pending || m_snapshot.availability != Availability::Ready || m_owner.isEmpty()) return {};
    bool admitted = false;
    for (const auto &row : m_snapshot.rows)
        if (row.attachment == attachment && available(row, action).enabled) admitted = true;
    if (!admitted) return {};
    ActionRequest request;
    request.requestId = QUuid::createUuid().toString(QUuid::Id128);
    request.lineage = m_snapshot.lineage;
    request.attachment = attachment;
    request.action = action;
    const auto encoded = encodeActionRequest(request);
    if (!encoded.succeeded()) return {};
    m_pending = RequestState{request, {}, {}, false};
    m_operationTimer.start(5000); // admission deadline; accepted work gets 2 minutes
    Q_EMIT snapshotChanged();
    auto call = QDBusMessage::createMethodCall(m_owner, QString::fromLatin1(kObjectPath),
        QString::fromLatin1(kInterfaceName), QStringLiteral("RequestAction"));
    call.setArguments({encoded.payload});
    call.setAutoStartService(false);
    const auto ownerSerial = m_ownerSerial;
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(call, 5000), this);
    m_admissionWatcher = watcher;
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
        [this, request, ownerSerial](QDBusPendingCallWatcher *pending) {
            const QDBusPendingReply<QByteArray> reply = *pending;
            pending->deleteLater();
            if (m_admissionWatcher == pending) m_admissionWatcher.clear();
            if (!m_pending || m_pending->request != request || ownerSerial != m_ownerSerial) return;
            ActionAdmission admission;
            if (!reply.isValid() || !decodeActionAdmission(reply.value(), admission).succeeded()
                || admission.request != request) {
                retireRequest(OperationStatus::Uncertain);
                return;
            }
            Q_EMIT admissionReceived(admission);
            if (!m_pending || m_pending->request != request) return;
            if (admission.status != AdmissionStatus::Accepted) {
                retireRequest(admission.status == AdmissionStatus::Busy ? OperationStatus::Busy
                    : admission.status == AdmissionStatus::Gone ? OperationStatus::Gone : OperationStatus::Refused);
                return;
            }
            m_pending->operationId = admission.operationId;
            m_operationTimer.start(120000);
            tryFinish();
        });
    return request.requestId;
}
void MediaClient::resultWire(const QByteArray &wire, const QDBusMessage &message)
{
    if (!m_pending || message.service() != m_owner) return;
    OperationResult result;
    if (!decodeOperationResult(wire, result).succeeded()) {
        retireRequest(OperationStatus::Uncertain);
        return;
    }
    if (result.request != m_pending->request) return;
    // A synchronous owner may finish before the admission reply reaches us.
    // Keep one bounded result until its operation id is authenticated by reply.
    if (!m_pending->operationId.isEmpty() && result.operationId != m_pending->operationId) return;
    m_pending->result = std::move(result);
    tryFinish();
}
void MediaClient::retireRequest(OperationStatus status)
{
    if (!m_pending) return;
    OperationResult result;
    result.request = m_pending->request;
    result.operationId = m_pending->operationId.isEmpty()
        ? QStringLiteral("unconfirmed_") + result.request.requestId : m_pending->operationId;
    result.status = status;
    result.diagnostic = {status == OperationStatus::Gone ? DiagnosticCode::Gone
        : status == OperationStatus::Busy ? DiagnosticCode::Busy
        : status == OperationStatus::Refused ? DiagnosticCode::NotAdmitted : DiagnosticCode::Uncertain,
        status == OperationStatus::Gone ? QStringLiteral("This media is no longer attached.")
        : status == OperationStatus::Busy ? QStringLiteral("Another media operation is still running.")
        : status == OperationStatus::Refused ? QStringLiteral("The media operation was refused. Refresh before trying again.")
        : QStringLiteral("The media state could not be confirmed. Refresh before trying again.")};
    m_pending.reset();
    m_operationTimer.stop();
    if (m_admissionWatcher) { m_admissionWatcher->deleteLater(); m_admissionWatcher.clear(); }
    Q_EMIT snapshotChanged();
    Q_EMIT operationFinished(result);
}
void MediaClient::tryFinish()
{
    if (!m_pending || m_pending->operationId.isEmpty() || !m_pending->result) return;
    const auto result = *m_pending->result;
    if (result.operationId != m_pending->operationId) { m_pending->result.reset(); return; }
    if (result.request.lineage.owner != m_owner || result.request.lineage.epoch != m_snapshot.lineage.epoch) {
        retireRequest(OperationStatus::Uncertain);
        return;
    }
    if (result.status == OperationStatus::Applied && result.confirmingRevision) {
        if (m_snapshot.lineage.revision < *result.confirmingRevision) {
            if (!m_pending->awaitingReadback) {
                m_pending->awaitingReadback = true;
                m_operationTimer.start(5000);
                requestSnapshot();
            }
            return;
        }
        const VolumeRow *row = nullptr;
        for (const auto &candidate : m_snapshot.rows)
            if (candidate.attachment == result.request.attachment) { row = &candidate; break; }
        const bool mounted = result.request.action == Action::Mount || result.request.action == Action::MountReadOnly;
        if (!row || (mounted && (row->mountState != MountState::Mounted || row->mountRoots.isEmpty()))
            || (result.request.action == Action::MountReadOnly && row->readOnly != ReadOnlyState::ReadOnly)
            || (result.request.action == Action::Unmount && (row->mountState != MountState::Unmounted || !row->mountRoots.isEmpty()))) {
            retireRequest(OperationStatus::Uncertain);
            return;
        }
    }
    m_pending.reset();
    m_operationTimer.stop();
    if (m_admissionWatcher) { m_admissionWatcher->deleteLater(); m_admissionWatcher.clear(); }
    Q_EMIT snapshotChanged();
    Q_EMIT operationFinished(result);
}
}
