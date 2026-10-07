// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_backend.h"
#include "media_controller.h"
#include <qindaqt/services/removable_media_protocol/media_types.h>
#include <QDBusConnection>

namespace QindaQt::Apps::RemovableMedia {
// Private owning adapter. Same-thread backend/controller/connection outlive
// the exporter. It never constructs a second watcher or reads choices. Invalid
// inventory is withdrawn atomically instead of truncating sibling partitions.
class MediaExporter final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.RemovableMedia1.Devices")
    Q_PROPERTY(uint Version READ version CONSTANT)
public:
    MediaExporter(MediaBackend &backend, MediaController &controller,
                  QDBusConnection connection, QObject *parent = nullptr);
    ~MediaExporter() override;
    [[nodiscard]] uint version() const { return QindaQt::RemovableMedia::kProtocolVersion; }
    [[nodiscard]] bool publishObject();
    [[nodiscard]] QindaQt::RemovableMedia::Snapshot snapshot() const { return m_snapshot; }
public Q_SLOTS:
    QByteArray GetSnapshot() const;
Q_SIGNALS:
    void SnapshotChanged(const QByteArray &wire);
private:
    void rebuild();
    MediaBackend &m_backend;
    MediaController &m_controller;
    QDBusConnection m_bus;
    QindaQt::RemovableMedia::Snapshot m_snapshot;
    QString m_epoch;
    quint64 m_authority = 0, m_revision = 0;
    bool m_registered = false;
};
}
