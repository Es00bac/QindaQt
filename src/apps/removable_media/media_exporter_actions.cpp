// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_exporter.h"
#include "media_public_projection.h"
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include <QDBusError>
#include <QUuid>

namespace QindaQt::Apps::RemovableMedia {
namespace Public = QindaQt::RemovableMedia;
namespace {
Operation privateOperation(Public::Action action)
{
    switch (action) {
    case Public::Action::Mount: return Operation::Mount;
    case Public::Action::MountReadOnly: return Operation::MountReadOnly;
    case Public::Action::Unmount: return Operation::Unmount;
    case Public::Action::Remove: return Operation::Remove;
    case Public::Action::ShowDetails: break;
    }
    return Operation::Mount; // ShowDetails never calls the backend.
}
Public::ActionAvailability availability(const Public::VolumeRow &row, Public::Action action)
{
    switch (action) {
    case Public::Action::Mount: return row.actions.mount;
    case Public::Action::MountReadOnly: return row.actions.mountReadOnly;
    case Public::Action::Unmount: return row.actions.unmount;
    case Public::Action::Remove: return row.actions.remove;
    case Public::Action::ShowDetails: return row.actions.showDetails;
    }
    return {};
}
Public::DiagnosticCode code(Public::OperationStatus status)
{
    switch (status) {
    case Public::OperationStatus::Applied: return Public::DiagnosticCode::None;
    case Public::OperationStatus::Cancelled: return Public::DiagnosticCode::Cancelled;
    case Public::OperationStatus::Busy: return Public::DiagnosticCode::Busy;
    case Public::OperationStatus::Gone: return Public::DiagnosticCode::Gone;
    case Public::OperationStatus::Uncertain: return Public::DiagnosticCode::Uncertain;
    default: return Public::DiagnosticCode::NotAdmitted;
    }
}
QString publicMessage(Public::OperationStatus status, Public::Action action)
{
    if (status == Public::OperationStatus::Applied) {
        if (action == Public::Action::Remove) return QStringLiteral("Safe to unplug.");
        if (action == Public::Action::Unmount) return QStringLiteral("Media unmounted.");
        if (action == Public::Action::ShowDetails) return QStringLiteral("Opened Removable Media.");
        return QStringLiteral("Media mounted.");
    }
    if (status == Public::OperationStatus::Busy) return QStringLiteral("Media is busy. Close files using it and try again.");
    if (status == Public::OperationStatus::Cancelled) return QStringLiteral("Authorization was cancelled.");
    if (status == Public::OperationStatus::Gone) return QStringLiteral("This media is no longer attached.");
    if (status == Public::OperationStatus::Uncertain) return QStringLiteral("The media state could not be confirmed. Refresh before trying again.");
    return QStringLiteral("The media operation was refused.");
}
}
QByteArray MediaExporter::RequestAction(const QByteArray &wire)
{
    Public::ActionRequest request;
    if (!Public::decodeActionRequest(wire, request).succeeded() || !calledFromDBus()) {
        if (calledFromDBus()) sendErrorReply(QDBusError::InvalidArgs, QStringLiteral("Invalid media request."));
        return {};
    }
    return Public::encodeActionAdmission(admit(request, message().service())).payload;
}
Public::ActionAdmission MediaExporter::admit(const Public::ActionRequest &request, const QString &caller)
{
    Public::ActionAdmission admission{request, Public::AdmissionStatus::Invalid, {},
        {Public::DiagnosticCode::Invalid, QStringLiteral("Invalid media request.")}};
    if (!Public::validateActionRequest(request).accepted() || caller.isEmpty()) return admission;
    const auto now = m_clock.elapsed();
    for (qsizetype i = m_recent.size(); i > 0; --i)
        if ((m_recent[i - 1].result || m_recent[i - 1].admission.status != Public::AdmissionStatus::Accepted) && now - m_recent[i - 1].started > Public::kRecentRequestLifetimeMilliseconds)
            m_recent.removeAt(i - 1);
    for (const auto &recent : std::as_const(m_recent)) {
        if (recent.caller == caller && recent.admission.request.lineage.epoch == request.lineage.epoch
            && recent.admission.request.requestId == request.requestId) {
            if (recent.admission.request != request) return admission;
            if (recent.result) Q_EMIT OperationFinished(Public::encodeOperationResult(*recent.result).payload);
            return recent.admission;
        }
    }
    const auto refuse = [&](Public::AdmissionStatus status, Public::DiagnosticCode reason, const QString &text) {
        admission.status = status;
        admission.diagnostic = {reason, text};
        if (m_recent.size() < Public::kMaxRecentRequests) m_recent.append({caller, now, admission, {}});
        return admission;
    };
    if (request.lineage.owner != m_snapshot.lineage.owner || request.lineage.epoch != m_epoch
        || request.lineage.revision != m_snapshot.lineage.revision)
        return refuse(Public::AdmissionStatus::Stale, Public::DiagnosticCode::Stale, QStringLiteral("The media inventory changed. Refresh before trying again."));
    if (!m_backend.available() || m_snapshot.availability != Public::Availability::Ready)
        return refuse(Public::AdmissionStatus::Unavailable, Public::DiagnosticCode::Unavailable, QStringLiteral("Media support is unavailable."));
    if (m_pending || m_controller.busy() || m_backend.busy() || m_recent.size() >= Public::kMaxRecentRequests)
        return refuse(Public::AdmissionStatus::Busy, Public::DiagnosticCode::Busy, QStringLiteral("Another media operation is still pending."));
    const auto volumes = m_backend.volumes();
    const Volume *selected = nullptr;
    for (const auto &volume : volumes)
        if (publicVolume(volume, m_epoch, false).attachment == request.attachment) { selected = &volume; break; }
    if (!selected)
        return refuse(Public::AdmissionStatus::Gone, Public::DiagnosticCode::Gone, QStringLiteral("This media is no longer attached."));
    const auto row = publicVolume(*selected, m_epoch, false);
    if (!availability(row, request.action).enabled)
        return refuse(Public::AdmissionStatus::NotAdmitted, Public::DiagnosticCode::NotAdmitted, QStringLiteral("This action is unavailable for the current media."));
    admission.status = Public::AdmissionStatus::Accepted;
    admission.operationId = QUuid::createUuid().toString(QUuid::Id128);
    admission.diagnostic = {};
    m_recent.append({caller, now, admission, {}});
    m_pending = Public::PendingOperation{admission.operationId, request, Public::ProgressPhase::Confirming};
    m_pendingToken = selected->token;
    m_pendingDrive = row.driveDisplayId;
    rebuild();
    if (request.action == Public::Action::ShowDetails) {
        m_controller.show(m_pendingToken);
        finishPublic(Public::OperationStatus::Applied);
    } else if (!m_controller.requestOrdinary(m_pendingToken, privateOperation(request.action))) {
        finishPublic(Public::OperationStatus::Refused);
    }
    return admission;
}
void MediaExporter::completed(const BackendCompletion &result)
{
    if (!m_pending || result.token != m_pendingToken
        || result.operation != privateOperation(m_pending->request.action)) return;
    finishPublic(result.status, result.removalMode);
}
void MediaExporter::finishPublic(Public::OperationStatus status, Public::RemovalMode mode)
{
    if (!m_pending) return;
    if (status == Public::OperationStatus::Applied && m_pending->request.action == Public::Action::Remove
        && mode == Public::RemovalMode::None)
        status = Public::OperationStatus::Uncertain;
    Public::OperationResult result;
    result.request = m_pending->request;
    result.operationId = m_pending->operationId;
    result.status = status;
    result.diagnostic = {code(status), publicMessage(status, result.request.action)};
    result.removalMode = status == Public::OperationStatus::Applied && result.request.action == Public::Action::Remove
        ? mode : Public::RemovalMode::None;
    m_pending.reset();
    m_pendingToken.clear();
    m_pendingDrive.clear();
    rebuild();
    if (status == Public::OperationStatus::Applied
        && (result.request.action == Public::Action::Mount || result.request.action == Public::Action::MountReadOnly
            || result.request.action == Public::Action::Unmount))
        result.confirmingRevision = m_snapshot.lineage.revision;
    for (auto &recent : m_recent)
        if (recent.admission.operationId == result.operationId) { recent.result = result; break; }
    const auto wire = Public::encodeOperationResult(result);
    if (wire.succeeded()) Q_EMIT OperationFinished(wire.payload);
}
}
