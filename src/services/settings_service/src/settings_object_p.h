// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_service/settings_repository.h"

#include <QDBusConnection>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtTypes>

namespace QindaQt::Services::SettingsService::Private {

// Private D-Bus adaptor exposing org.qindaqt.Settings1. Decodes/bounds every
// request via SettingsProtocol before it ever reaches the repository, maps
// RepositoryCommitStatus to the wire SettingsWireStatus, and broadcasts
// SettingsChanged after a real (non-no-op) Applied commit. Owns no settings
// state itself -- the repository is borrowed and must outlive this object.
//
// AGENT-CONTRACT: Settings1 has no per-call authorization boundary. Any
// same-session client may read or commit user-override values; "owner-
// authenticated" (see settings_wire_contract.h) refers to the *client*
// fencing stale/late replies and signals to one exact unique service owner,
// not to this object restricting who may call it. Every outcome -- including
// conflict, validation failure, and malformed requests -- is a normal typed
// reply; this object never calls sendErrorReply.
class SettingsObject final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.qindaqt.Settings1")

public:
    SettingsObject(QDBusConnection connection, SettingsRepository &repository, QObject *parent = nullptr);

public slots:
    [[nodiscard]] QVariantMap GetSnapshot(const QStringList &keys);
    [[nodiscard]] QVariantMap CommitUserTransaction(const QString &epoch, quint64 baseRevision,
                                                     const QVariantList &operations);

private:
    [[nodiscard]] QVariantMap encodeSnapshot(const RepositorySnapshot &snapshot,
                                             QVariantMap wireValues) const;
    [[nodiscard]] QVariantMap encodeCommitResult(const RepositoryCommitResult &result,
                                                 QVariantMap wireValues) const;
    void publishChanged(const RepositoryCommitResult &result);

    QDBusConnection m_connection;
    SettingsRepository &m_repository;
};

} // namespace QindaQt::Services::SettingsService::Private
