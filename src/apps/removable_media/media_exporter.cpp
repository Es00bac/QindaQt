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
    if (m_authority != m_backend.authorityGeneration()) {
        m_authority = m_backend.authorityGeneration();
        m_epoch = QUuid::createUuid().toString(QUuid::Id128);
        m_revision = 0;
    }
    Public::Snapshot next;
    next.lineage = {m_bus.baseService(), m_epoch, ++m_revision};
    if (m_backend.available()) {
        next.availability = Public::Availability::Ready;
        for (const auto &volume : m_backend.volumes())
            next.rows.append(publicVolume(volume, m_epoch, m_controller.busy()));
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
}
}
