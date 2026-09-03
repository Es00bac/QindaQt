// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/applets/api_version.h"
#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/services/settings_client/settings_transport.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QDir>
#include <QJsonObject>
#include <QRect>
#include <QTemporaryDir>

#include <memory>
#include <utility>

namespace QindaQt::Apps::SettingsCustomize::TestSupport {

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

inline Profiles::LayoutProfile profile(QString id = QStringLiteral("fixture"))
{
    Profiles::LayoutProfile result;
    result.id = std::move(id);
    result.name = result.id == QLatin1String("fixture")
        ? QStringLiteral("Fixture") : QStringLiteral("Alternate");

    Profiles::PanelSpec bar;
    bar.id = QStringLiteral("bar");
    bar.output = QStringLiteral("primary");
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
    dock.output = QStringLiteral("primary");
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

inline Applets::AppletManifest manifest(QString id)
{
    Applets::AppletManifest result;
    result.id = std::move(id);
    result.name = result.id;
    result.apiVersion = Applets::ApiVersion::current();
    result.entryPoint = {Applets::EntryPointKind::Builtin, result.id};
    result.placementZones = {Applets::PlacementZone::PanelStart,
                             Applets::PlacementZone::PanelCenter,
                             Applets::PlacementZone::PanelEnd};
    result.orientations = {Applets::Orientation::Horizontal,
                           Applets::Orientation::Vertical};
    result.sizing.mainAxis = {16, 32, 256, false};
    result.sizing.crossAxis = {16, 32, 128, false};
    result.settingsSchema = {
        {QStringLiteral("type"), QStringLiteral("object")},
        {QStringLiteral("properties"),
         QJsonObject{{QStringLiteral("labelFormat"),
                      QJsonObject{{QStringLiteral("type"),
                                   QStringLiteral("string")},
                                  {QStringLiteral("default"),
                                   QStringLiteral("short")}}}}},
    };
    return result;
}

inline QVector<Applets::AppletManifest> manifests()
{
    return {manifest(QStringLiteral("launcher")),
            manifest(QStringLiteral("clock")),
            manifest(QStringLiteral("task-list"))};
}

inline QVector<ShellLayout::LogicalOutput> outputs()
{
    return {{QStringLiteral("primary"), QRect(0, 0, 1920, 1080), 1.0}};
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

inline QVariantMap commitWire(
    Services::SettingsProtocol::SettingsWireStatus status,
    const QString &profileId,
    const QString &message = {})
{
    using Services::SettingsProtocol::WireContract;
    const QVariantMap values{{QStringLiteral("panels.layoutProfile"), profileId}};
    const QVariantMap sources{{QStringLiteral("panels.layoutProfile"),
                               QStringLiteral("user-overrides")}};
    const quint64 before = status
            == Services::SettingsProtocol::SettingsWireStatus::Applied
        ? 7 : 8;
    const quint64 after = status
            == Services::SettingsProtocol::SettingsWireStatus::Applied
        ? 8 : 7;
    return {{QLatin1StringView(WireContract::FieldStatus), quint32(status)},
            {QLatin1StringView(WireContract::FieldWireSchemaVersion),
             WireContract::WireSchemaVersion},
            {QLatin1StringView(WireContract::FieldSettingsSchemaVersion), quint32(2)},
            {QLatin1StringView(WireContract::FieldEpoch), QStringLiteral("epoch-a")},
            {QLatin1StringView(WireContract::FieldRevisionBefore), before},
            {QLatin1StringView(WireContract::FieldRevisionAfter),
             status == Services::SettingsProtocol::SettingsWireStatus::Applied
                 ? after : before},
            {QLatin1StringView(WireContract::FieldValues), values},
            {QLatin1StringView(WireContract::FieldSourceLayers), sources},
            {QLatin1StringView(WireContract::FieldChangedKeys), QStringList{}},
            {QLatin1StringView(WireContract::FieldMessage), message}};
}

inline std::unique_ptr<QTemporaryDir> temporaryStore(const QString &name)
{
    auto directory = std::make_unique<QTemporaryDir>(
        QDir::current().filePath(name + QStringLiteral("-XXXXXX")));
    return directory;
}

} // namespace QindaQt::Apps::SettingsCustomize::TestSupport
