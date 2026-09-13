// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/services/settings_client/settings_transport.h"

#include <QList>
#include <QString>
#include <QVariantList>

namespace QindaQt::Apps::SettingsCustomize::TestSupport {

// A deterministic, in-process SettingsTransport double: records every
// request instead of reaching D-Bus, so a test drives Settings1 confirmation
// and invalidation by hand through snapshotReceived()/settingsChanged().
// Shared by every Customize test scope that needs its own SettingsClient
// (panel/profile, wallpaper, and contained-window-preview theme/chrome).
class SequenceTransport final
    : public Services::SettingsClient::SettingsTransport {
    Q_OBJECT

public:
    bool start(QString *) override { return true; }
    void stop() override {}
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &) override
    {
        snapshots.append({token, owner});
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &operations) override
    {
        commits.append({token, owner, epoch, revision, operations});
    }
    void requestActivation() override {}

    struct SnapshotRequest final {
        quint64 token = 0;
        QString owner;
    };
    struct CommitRequest final {
        quint64 token = 0;
        QString owner;
        QString epoch;
        quint64 revision = 0;
        QVariantList operations;
    };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
};

} // namespace QindaQt::Apps::SettingsCustomize::TestSupport
