// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "customize_sequence_transport.h"

#include "qindaqt/apps/settings_customize/customize_settings_model.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/profiles/user_profile_store.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <utility>

namespace QindaQt::Apps::SettingsCustomize::TestSupport {

inline Profiles::LayoutProfile profile(QString id = QStringLiteral("fixture"),
                                       QString name = {})
{
    Profiles::LayoutProfile result;
    result.id = std::move(id);
    result.name = !name.isEmpty() ? std::move(name)
        : result.id == QLatin1String("fixture") ? QStringLiteral("Fixture")
                                                : QStringLiteral("Alternate");
    result.description = QStringLiteral("A test layout");

    Profiles::PanelSpec bar;
    bar.id = QStringLiteral("bar");
    bar.edge = Profiles::Edge::Top;
    bar.applets = {
        {.id = QStringLiteral("launcher-instance"),
         .plugin = QStringLiteral("launcher"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("start")}}},
        {.id = QStringLiteral("clock-instance"),
         .plugin = QStringLiteral("clock"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("end")}}},
    };

    Profiles::PanelSpec dock;
    dock.id = QStringLiteral("dock");
    dock.edge = Profiles::Edge::Bottom;
    dock.layer = Profiles::Layer::Overlay;
    dock.alignment = Profiles::Alignment::Center;
    dock.length = 0.6;
    dock.thickness = 48;
    dock.applets = {
        {.id = QStringLiteral("tasks-instance"),
         .plugin = QStringLiteral("task-list"),
         .settings = {{QStringLiteral("zone"), QStringLiteral("center")}}},
    };
    result.panels = {bar, dock};
    return result;
}

inline QVariantMap snapshotWire(const QString &profileId,
                                const QString &epoch = QStringLiteral("epoch-a"),
                                quint64 revision = 7)
{
    using Services::SettingsProtocol::SettingsWireStatus;
    using Services::SettingsProtocol::WireContract;
    const QVariantMap values{{QStringLiteral("panels.layoutProfile"), profileId}};
    const QVariantMap sources{{QStringLiteral("panels.layoutProfile"),
                               QStringLiteral("user-overrides")}};
    return {{QLatin1StringView(WireContract::FieldStatus),
             quint32(SettingsWireStatus::Applied)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), epoch},
            {QLatin1StringView(WireContract::FieldRevision), revision},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldMessage), QString{}}};
}

inline QVariantMap panelDelaySnapshotWire(
    const QString &profileId, qint64 delayMs,
    const QString &epoch = QStringLiteral("epoch-a"),
    quint64 revision = 7)
{
    using Services::SettingsProtocol::WireContract;
    QVariantMap wire = snapshotWire(profileId, epoch, revision);
    QVariantMap values = wire.value(QLatin1StringView(WireContract::FieldValues)).toMap();
    QVariantMap sources = wire.value(QLatin1StringView(WireContract::FieldSourceLayers)).toMap();
    values.insert(QString(PanelHideDelaySettingsKey), delayMs);
    sources.insert(QString(PanelHideDelaySettingsKey),
                   QStringLiteral("user-overrides"));
    wire.insert(QLatin1StringView(WireContract::FieldValues), values);
    wire.insert(QLatin1StringView(WireContract::FieldSourceLayers), sources);
    return wire;
}

// A layout-selection commit reply against a request made at revision 7, with
// the revision pair each status must carry to pass the client's reply
// validation: Applied moves 7 -> 8, Conflict reports the newer 8 it lost to,
// every other refusal stays at 7.
inline QVariantMap commitWire(
    Services::SettingsProtocol::SettingsWireStatus status,
    const QString &profileId,
    const QString &message = {})
{
    using Services::SettingsProtocol::SettingsWireStatus;
    using Services::SettingsProtocol::WireContract;
    const bool applied = status == SettingsWireStatus::Applied;
    const quint64 before = status == SettingsWireStatus::Conflict ? 8 : 7;
    const quint64 after = applied ? 8 : before;
    const QVariantMap values{{QStringLiteral("panels.layoutProfile"), profileId}};
    const QVariantMap sources{{QStringLiteral("panels.layoutProfile"),
                               QStringLiteral("user-overrides")}};
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch-a")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter), after},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldChangedKeys),
             applied ? QStringList{QString(LayoutProfileSettingsKey)} : QStringList{}},
            {QLatin1StringView(WireContract::FieldMessage), message}};
}

inline std::unique_ptr<QTemporaryDir> temporaryStore(const QString &name)
{
    return std::make_unique<QTemporaryDir>(
        QDir::current().filePath(name + QStringLiteral("-XXXXXX")));
}

// Installs `profiles` as the built-in catalog of `directory` through the
// profiles module's own strict writer, so every fixture is a valid schema-v1
// document the production loader accepts.
inline bool installProfiles(const QString &directory,
                            const QVector<Profiles::LayoutProfile> &profiles)
{
    const Profiles::UserProfileStore writer(directory);
    for (const auto &entry : profiles) {
        if (!writer.save(entry).ok()) {
            return false;
        }
    }
    return true;
}

// The production model over an in-process Settings1 double, one temporary
// installed catalog (Fixture, Alternate and the default Mac layout) and a
// user store that does not exist until the first save, like a new account.
class ModelHarness final {
public:
    explicit ModelHarness(bool enablePanelDelay = false)
        : includePanelDelay(enablePanelDelay)
        , stock(temporaryStore(QStringLiteral("customize-stock")))
        , home(temporaryStore(QStringLiteral("customize-home")))
        , installed(stock->isValid() && home->isValid()
                    && installProfiles(stock->path(),
                                       {profile(), profile(QStringLiteral("alternate")),
                                        profile(QString(DefaultLayoutPresetId),
                                                QStringLiteral("Mac"))}))
        , client(transport,
                 enablePanelDelay
                     ? QStringList{QString(LayoutProfileSettingsKey),
                                   QString(PanelHideDelaySettingsKey)}
                     : QStringList{QString(LayoutProfileSettingsKey)},
                 {.requestTimeoutMilliseconds = 100,
                  .debounceMilliseconds = 0,
                  .retryMilliseconds = {10}})
        , model(client, {{stock->path()}, userDirectory()})
    {
    }

    [[nodiscard]] QString userDirectory() const
    {
        return QDir(home->path()).filePath(QStringLiteral("profiles"));
    }

    [[nodiscard]] QString userFile(const QString &id) const
    {
        return QDir(userDirectory()).filePath(
            Profiles::UserProfileStore::fileNameForId(id));
    }

    // Writes a user-store copy the way the shell's panel edits do.
    bool writeUserCopy(const Profiles::LayoutProfile &copy)
    {
        return Profiles::UserProfileStore(userDirectory()).save(copy).ok();
    }

    bool establish(const QString &profileId = QStringLiteral("fixture"),
                   qint64 panelDelayMs = 250)
    {
        if (!installed || !client.start()) {
            return false;
        }
        Q_EMIT transport.ownerChanged(QStringLiteral(":1.90"));
        if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); }, 5'000)) {
            return false;
        }
        const auto request = takeNewestSnapshotRequest();
        Q_EMIT transport.snapshotReceived(
            request.token, request.owner,
            includePanelDelay ? panelDelaySnapshotWire(profileId, panelDelayMs)
                              : snapshotWire(profileId));
        return QTest::qWaitFor([this] { return model.ready(); }, 5'000);
    }

    // The client fences every reply to its newest request token (an older
    // request may already have timed out and been retried), so answer that.
    SequenceTransport::SnapshotRequest takeNewestSnapshotRequest()
    {
        const auto request = transport.snapshots.takeLast();
        transport.snapshots.clear();
        return request;
    }

    // Answers the pending snapshot request.
    bool replySnapshot(const QString &profileId, quint64 revision,
                       const QString &epoch = QStringLiteral("epoch-a"),
                       qint64 panelDelayMs = 250)
    {
        if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); }, 5'000)) {
            return false;
        }
        const auto request = takeNewestSnapshotRequest();
        Q_EMIT transport.snapshotReceived(
            request.token, request.owner,
            includePanelDelay ? panelDelaySnapshotWire(profileId, panelDelayMs, epoch, revision)
                              : snapshotWire(profileId, epoch, revision));
        return true;
    }

    // Answers the oldest pending commit with a layout-selection reply.
    bool replyCommit(Services::SettingsProtocol::SettingsWireStatus status,
                     const QString &profileId, const QString &message = {})
    {
        if (transport.commits.isEmpty()) {
            return false;
        }
        const auto commit = transport.commits.takeFirst();
        Q_EMIT transport.commitReceived(commit.token, commit.owner,
                                        commitWire(status, profileId, message));
        return true;
    }

    bool includePanelDelay = false;
    std::unique_ptr<QTemporaryDir> stock;
    std::unique_ptr<QTemporaryDir> home;
    bool installed = false;
    SequenceTransport transport;
    Services::SettingsClient::SettingsClient client;
    CustomizeSettingsModel model;
};

} // namespace QindaQt::Apps::SettingsCustomize::TestSupport
