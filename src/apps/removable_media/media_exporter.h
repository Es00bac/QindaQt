// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "media_backend.h"
#include "media_controller.h"
#include <qindaqt/services/removable_media_protocol/media_types.h>
#include <QDBusConnection>
#include <QDBusContext>
#include <QElapsedTimer>

namespace QindaQt::Apps::RemovableMedia {
// Private owning adapter. Same-thread backend/controller/connection outlive
// the exporter. It never constructs a second watcher or reads choices. Invalid
// inventory is withdrawn atomically instead of truncating sibling partitions.
class MediaExporter final : public QObject, protected QDBusContext {
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
    // Owning C++ admission seam for private fixtures; never exported as a slot.
    [[nodiscard]] QindaQt::RemovableMedia::ActionAdmission admit(
        const QindaQt::RemovableMedia::ActionRequest &request, const QString &caller);
public Q_SLOTS:
    QByteArray GetSnapshot() const;
    QByteArray RequestAction(const QByteArray &wire);
Q_SIGNALS:
    void SnapshotChanged(const QByteArray &wire);
    void OperationFinished(const QByteArray &wire);
private:
    void rebuild();
    void completed(const BackendCompletion &result);
    void finishPublic(QindaQt::RemovableMedia::OperationStatus status,
                      QindaQt::RemovableMedia::RemovalMode mode = QindaQt::RemovableMedia::RemovalMode::None);
    struct Recent {
        QString caller;
        qint64 started = 0;
        QindaQt::RemovableMedia::ActionAdmission admission;
        std::optional<QindaQt::RemovableMedia::OperationResult> result;
    };
    QVector<Recent> m_recent;
    QElapsedTimer m_clock;
    std::optional<QindaQt::RemovableMedia::PendingOperation> m_pending;
    QString m_pendingToken, m_pendingDrive;
    MediaBackend &m_backend;
    MediaController &m_controller;
    QDBusConnection m_bus;
    QindaQt::RemovableMedia::Snapshot m_snapshot;
    QString m_epoch;
    quint64 m_authority = 0, m_revision = 0;
    bool m_registered = false;
};
}
