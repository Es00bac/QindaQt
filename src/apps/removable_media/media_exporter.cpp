// SPDX-License-Identifier: GPL-3.0-or-later
#include "media_exporter.h"
#include "media_public_projection.h"
#include <qindaqt/services/removable_media_protocol/media_codec.h>
#include <QUuid>
#include <utility>

namespace QindaQt::Apps::RemovableMedia {
namespace Public = QindaQt::RemovableMedia;
MediaExporter::MediaExporter(MediaBackend &backend, MediaController &controller,
                             QDBusConnection bus, QObject *parent)
    : QObject(parent), m_backend(backend), m_controller(controller), m_bus(std::move(bus)),
      m_epoch(QUuid::createUuid().toString(QUuid::Id128)), m_authority(backend.authorityGeneration())
{
    m_clock.start();
    connect(&backend, &MediaBackend::operationCompleted, this, &MediaExporter::completed);
    connect(&backend, &MediaBackend::changed, this, &MediaExporter::rebuild);
    connect(&controller, &MediaController::changed, this, &MediaExporter::rebuild);
    rebuild();
}
MediaExporter::~MediaExporter()
{
    if (m_registered) m_bus.unregisterObject(QString::fromLatin1(Public::kObjectPath));
}
bool MediaExporter::publishObject()
{
    m_registered = m_bus.registerObject(QString::fromLatin1(Public::kObjectPath), this,
        QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals | QDBusConnection::ExportAllProperties);
    return m_registered;
}
QByteArray MediaExporter::GetSnapshot() const { return Public::encodeSnapshot(m_snapshot).payload; }
void MediaExporter::rebuild()
{
    std::optional<Public::OperationResult> retired;
    if (m_authority != m_backend.authorityGeneration()) {
        if (m_pending) {
            retired = Public::OperationResult{m_pending->request, m_pending->operationId,
                Public::OperationStatus::Uncertain, {Public::DiagnosticCode::Uncertain,
                QStringLiteral("The media authority changed. Refresh before trying again.")}, {}, Public::RemovalMode::None};
            m_pending.reset();
            m_pendingToken.clear();
            m_pendingDrive.clear();
        }
        m_recent.clear();
        m_authority = m_backend.authorityGeneration();
        m_epoch = QUuid::createUuid().toString(QUuid::Id128);
        m_revision = 0;
    }
    Public::Snapshot next;
    next.lineage = {m_bus.baseService(), m_epoch, ++m_revision};
    if (m_backend.available()) {
        next.availability = Public::Availability::Ready;
        for (const auto &volume : m_backend.volumes()) {
            auto row = publicVolume(volume, m_epoch, m_controller.busy() || m_pending.has_value());
            if (m_backend.busy() && volume.driveIdentity == m_backend.pendingDriveIdentity())
                row.progress = m_backend.phase();
            next.rows.append(std::move(row));
        }
        if (m_pending) {
            auto pending = *m_pending;
            pending.phase = m_backend.phase() == Public::ProgressPhase::Idle
                ? Public::ProgressPhase::Confirming : m_backend.phase();
            next.pending = std::move(pending);
        }
    } else {
        next.availability = Public::Availability::Unavailable;
        next.diagnostic = {Public::DiagnosticCode::Unavailable,
            QStringLiteral("The system disk service is unavailable. Open Removable Media to refresh.")};
    }
    if (!Public::encodeSnapshot(next).succeeded()) {
        next.rows.clear();
        next.pending.reset();
        next.availability = Public::Availability::Unavailable;
        next.diagnostic = {Public::DiagnosticCode::Invalid,
            QStringLiteral("The media inventory exceeds supported bounds or is invalid. Open Removable Media.")};
    }
    m_snapshot = std::move(next);
    Q_EMIT SnapshotChanged(GetSnapshot());
    if (retired) Q_EMIT OperationFinished(Public::encodeOperationResult(*retired).payload);
}
}
