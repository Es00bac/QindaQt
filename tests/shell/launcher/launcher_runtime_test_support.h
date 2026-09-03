// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Shared fixtures for the launcher L1 adapter tests: desktop-entry fixture
// trees, a recording spawner/activator, and a scripted Settings1 transport.
// AGENT-CONTRACT: These helpers must never start real applications, touch the
// host session bus, or read the user's real data roots.

#include "launch_activator.h"
#include "launch_spawner.h"
#include "launcher_persistence.h"

#include <qindaqt/services/settings_client/settings_transport.h>
#include <qindaqt/services/settings_protocol/settings_wire_contract.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QDir>
#include <QFile>
#include <QObject>
#include <QTemporaryDir>
#include <QVariantMap>

namespace QindaQt::Tests::Launcher {

inline bool writeDesktopFile(const QString &root, const QString &relativePath,
                             const QString &text)
{
    const QString path = root + QStringLiteral("/applications/") + relativePath;
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write(text.toUtf8());
    return true;
}

inline QString minimalEntry(const QString &name, const QString &exec,
                            const QString &extra = {})
{
    QString text = QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\n")
                       .arg(name);
    if (!exec.isEmpty())
        text += QStringLiteral("Exec=%1\n").arg(exec);
    if (!extra.isEmpty())
        text += extra;
    return text;
}

class RecordingSpawner final : public Shell::Launcher::LaunchSpawner {
public:
    Shell::Launcher::SpawnResult spawn(
        const Shell::Launcher::SpawnRequest &request) override
    {
        requests.append(request);
        return nextResult;
    }

    QList<Shell::Launcher::SpawnRequest> requests;
    Shell::Launcher::SpawnResult nextResult { true, {} };
};

// No Q_OBJECT: the fake only emits the base class's signals and overrides its
// virtuals, so it needs no metaobject of its own (same pattern as the power
// client test fakes).
class RecordingActivator final : public Shell::Launcher::LaunchActivator {
public:
    using Shell::Launcher::LaunchActivator::LaunchActivator;

    Shell::Launcher::ActivationDispatch activate(const QString &desktopId,
                                                 const QString &actionId) override
    {
        activations.append({ desktopId, actionId });
        if (!accept)
            return { false, QStringLiteral("activation refused by fixture") };
        return { true, {} };
    }

    void finish(const QString &desktopId, bool ok, const QString &diagnostic = {})
    {
        Q_EMIT activationFinished(desktopId, ok, diagnostic);
    }

    struct Activation { QString desktopId; QString actionId; };
    QList<Activation> activations;
    bool accept = true;
};

// Scripted Settings1 transport: records requests; the test drives replies
// through replySnapshot/replyCommit and owner lifecycle helpers.
class FakeSettingsTransport final
    : public Services::SettingsClient::SettingsTransport {
public:
    using Services::SettingsClient::SettingsTransport::SettingsTransport;

    bool start(QString *error) override
    {
        ++starts;
        if (!startSucceeds) {
            if (error != nullptr)
                *error = QStringLiteral("transport unavailable");
            return false;
        }
        started = true;
        return true;
    }
    void stop() override { started = false; }
    void requestSnapshot(quint64 token, const QString &owner,
                         const QStringList &keys) override
    {
        snapshots.append({ token, owner, keys });
    }
    void commit(quint64 token, const QString &owner, const QString &epoch,
                quint64 revision, const QVariantList &operations) override
    {
        commits.append({ token, owner, epoch, revision, operations });
    }
    void requestActivation() override { ++activations; }

    void announceOwner(const QString &owner = QStringLiteral(":1.99"))
    {
        Q_EMIT ownerChanged(owner);
    }

    // The client requires exact scope: every requested key must appear in the
    // reply. Keys the fixture does not set are reported as canonical null,
    // which the launcher controller treats as absent.
    static QVariantMap snapshotWire(const QString &epoch, quint64 revision,
                                    const QVariantMap &values)
    {
        using Services::SettingsProtocol::SettingsWireStatus;
        using Services::SettingsProtocol::WireContract;
        QVariantMap full = values;
        for (const QString &key :
             { Shell::Launcher::LauncherPersistenceController::pinnedKey(),
               Shell::Launcher::LauncherPersistenceController::recentKey() }) {
            if (!full.contains(key))
                full.insert(key, QVariant::fromValue(nullptr));
        }
        QVariantMap sources;
        for (auto it = full.cbegin(); it != full.cend(); ++it)
            sources.insert(it.key(), QStringLiteral("user-overrides"));
        return {
            { QLatin1StringView(WireContract::FieldStatus),
              quint32(SettingsWireStatus::Applied) },
            { QLatin1StringView(WireContract::FieldWireSchemaVersion),
              WireContract::WireSchemaVersion },
            { QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2) },
            { QLatin1StringView(WireContract::FieldEpoch), epoch },
            { QLatin1StringView(WireContract::FieldRevision), revision },
            { QLatin1StringView(WireContract::FieldValues), full },
            { QLatin1StringView(WireContract::FieldSourceLayers), sources },
            { QLatin1StringView(WireContract::FieldMessage), QString {} },
        };
    }

    static QVariantMap commitWire(Services::SettingsProtocol::SettingsWireStatus status,
                                  const QString &epoch, quint64 before, quint64 after,
                                  const QVariantMap &values)
    {
        using Services::SettingsProtocol::SettingsWireStatus;
        using Services::SettingsProtocol::WireContract;
        const bool applied = status == SettingsWireStatus::Applied && after == before + 1;
        QVariantMap sources;
        QStringList changed;
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            sources.insert(it.key(), QStringLiteral("user-overrides"));
            if (applied)
                changed.append(it.key());
        }
        return {
            { QLatin1StringView(WireContract::FieldStatus), quint32(status) },
            { QLatin1StringView(WireContract::FieldWireSchemaVersion),
              WireContract::WireSchemaVersion },
            { QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2) },
            { QLatin1StringView(WireContract::FieldEpoch), epoch },
            { QLatin1StringView(WireContract::FieldRevisionBefore), before },
            { QLatin1StringView(WireContract::FieldRevisionAfter), after },
            { QLatin1StringView(WireContract::FieldValues),
              status == SettingsWireStatus::UnknownKey ? QVariantMap {} : values },
            { QLatin1StringView(WireContract::FieldSourceLayers),
              status == SettingsWireStatus::UnknownKey ? QVariantMap {} : sources },
            { QLatin1StringView(WireContract::FieldChangedKeys), changed },
            { QLatin1StringView(WireContract::FieldMessage), QString {} },
        };
    }

    void replyLastSnapshot(const QVariantMap &wire)
    {
        const auto request = snapshots.constLast();
        Q_EMIT snapshotReceived(request.token, request.owner, wire);
    }
    void replyLastCommit(const QVariantMap &wire)
    {
        const auto request = commits.constLast();
        Q_EMIT commitReceived(request.token, request.owner, wire);
    }

    struct SnapshotRequest { quint64 token; QString owner; QStringList keys; };
    struct CommitRequest {
        quint64 token; QString owner; QString epoch; quint64 revision;
        QVariantList operations;
    };
    QList<SnapshotRequest> snapshots;
    QList<CommitRequest> commits;
    int activations = 0;
    int starts = 0;
    bool startSucceeds = true;
    bool started = false;
};

} // namespace QindaQt::Tests::Launcher
