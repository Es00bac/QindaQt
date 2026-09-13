// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "customize_sequence_transport.h"
#include "customize_window_preview_test_support.h"

#include "qindaqt/app_appearance/application_appearance_controller.h"
#include "qindaqt/applets/api_version.h"
#include "qindaqt/applets/applet_manifest.h"
#include "qindaqt/apps/settings_customize/customize_editor_host.h"
#include "qindaqt/apps/settings_customize/customize_output_provider.h"
#include "qindaqt/apps/settings_customize/customize_settings_model.h"
#include "qindaqt/apps/settings_customize/customize_wallpaper_preview.h"
#include "qindaqt/apps/settings_customize/customize_window_preview.h"
#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"
#include "qindaqt/shell_layout/panel_layout_types.h"

#include <QDir>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QRect>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>
#include <utility>

namespace QindaQt::Apps::SettingsCustomize::TestSupport {

class MutableCustomizeOutputProvider final : public CustomizeOutputProvider {
public:
    explicit MutableCustomizeOutputProvider(QObject *parent = nullptr)
        : CustomizeOutputProvider(parent)
    {
        m_snapshot.outputs = {
            {QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0},
            {QStringLiteral("HDMI-A-1"), QRect(1920, 0, 2560, 1440), 1.25},
        };
        m_snapshot.primaryOutputIds = {QStringLiteral("DP-1")};
        m_snapshot.revision = 1;
    }

    [[nodiscard]] CustomizeOutputSnapshot snapshot() const override
    {
        return m_snapshot;
    }

    void publish(CustomizeOutputSnapshot snapshot)
    {
        m_snapshot = std::move(snapshot);
        Q_EMIT snapshotChanged();
    }

private:
    CustomizeOutputSnapshot m_snapshot;
};

inline Profiles::LayoutProfile profile(QString id = QStringLiteral("fixture"))
{
    Profiles::LayoutProfile result;
    result.id = std::move(id);
    result.name = result.id == QLatin1String("fixture")
        ? QStringLiteral("Fixture") : QStringLiteral("Alternate");

    Profiles::PanelSpec bar;
    bar.id = QStringLiteral("bar");
    bar.output = QStringLiteral("DP-1");
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
    dock.output = QStringLiteral("DP-1");
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
         QJsonObject{
             // Freeform string with no enum: deliberately Unsupported, the
             // read-only negative control (customize_applet_setting_validation.h).
             {QStringLiteral("labelFormat"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                          {QStringLiteral("default"), QStringLiteral("short")}}},
             {QStringLiteral("showIcon"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                          {QStringLiteral("default"), true}}},
             {QStringLiteral("refreshSeconds"),
              QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                          {QStringLiteral("minimum"), 1},
                          {QStringLiteral("maximum"), 60},
                          {QStringLiteral("default"), 5}}},
             {QStringLiteral("alignment"),
              QJsonObject{
                  {QStringLiteral("type"), QStringLiteral("string")},
                  {QStringLiteral("enum"),
                   QJsonArray{QStringLiteral("leading"), QStringLiteral("center"),
                             QStringLiteral("trailing")}},
                  {QStringLiteral("default"), QStringLiteral("leading")}}},
         }},
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
    return {
        {QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0},
        {QStringLiteral("HDMI-A-1"), QRect(1920, 0, 2560, 1440), 1.25},
    };
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

inline QVariantMap wallpaperSnapshotWire(
    const QString &wallpaper,
    const QString &mode,
    const QString &epoch = QStringLiteral("epoch-a"),
    quint64 revision = 7)
{
    using Services::SettingsProtocol::SettingsWireStatus;
    using Services::SettingsProtocol::WireContract;
    const QVariantMap values{{QStringLiteral("appearance.wallpaper"), wallpaper},
                             {QStringLiteral("appearance.wallpaperMode"), mode}};
    const QVariantMap sources{{QStringLiteral("appearance.wallpaper"),
                               QStringLiteral("user-overrides")},
                              {QStringLiteral("appearance.wallpaperMode"),
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

class ModelHarness final {
public:
    ModelHarness()
        : store(temporaryStore(QStringLiteral("customize-model")))
        , wallpaperStore(temporaryStore(QStringLiteral("customize-wallpapers")))
        , client(transport, {QString(LayoutProfileSettingsKey)},
                 {.requestTimeoutMilliseconds = 100,
                  .debounceMilliseconds = 0,
                  .retryMilliseconds = {10}})
        , wallpaperClient(wallpaperTransport,
                          {QStringLiteral("appearance.wallpaper"),
                           QStringLiteral("appearance.wallpaperMode")},
                          {.requestTimeoutMilliseconds = 100,
                           .debounceMilliseconds = 0,
                           .retryMilliseconds = {10}})
        , wallpaperPreview(wallpaperClient, {wallpaperStore->path()})
        , appearance(windowPreviewTransports.themeClient, themeDirectories(),
                     QStringLiteral("qinda-dark"))
        , windowPreview(appearance, windowPreviewTransports.chromeClient)
        , model(client, {profile(), profile(QStringLiteral("alternate"))},
                manifests(), outputProvider, wallpaperPreview, windowPreview,
                [this](const Profiles::LayoutProfile &selected,
                       const QVector<ShellLayout::LogicalOutput> &inventory) {
                    return std::make_unique<RepositoryCustomizeEditorHost>(
                        selected, inventory, manifests(), store->path());
                })
    {
    }

    bool establish(const QString &profileId = QStringLiteral("fixture"),
                   const QString &wallpaper = {},
                   const QString &wallpaperMode = QStringLiteral("scaled"))
    {
        if (!store->isValid() || !wallpaperStore->isValid() || !client.start()
            || !wallpaperClient.start()) {
            return false;
        }
        // Seed one bundled wallpaper so qindaqt: identities can resolve.
        const QString wallpaperPath = wallpaperStore->path()
            + QStringLiteral("/fixture-wall.png");
        QImage seed(8, 8, QImage::Format_ARGB32);
        seed.fill(Qt::transparent);
        if (!seed.save(wallpaperPath)) {
            return false;
        }
        Q_EMIT transport.ownerChanged(QStringLiteral(":1.90"));
        Q_EMIT wallpaperTransport.ownerChanged(QStringLiteral(":1.90"));
        if (!QTest::qWaitFor([this] { return !transport.snapshots.isEmpty(); },
                             5'000)
            || !QTest::qWaitFor(
                [this] { return !wallpaperTransport.snapshots.isEmpty(); },
                5'000)) {
            return false;
        }
        const auto request = transport.snapshots.takeFirst();
        Q_EMIT transport.snapshotReceived(request.token, request.owner,
                                          snapshotWire(profileId));
        const auto wallpaperRequest = wallpaperTransport.snapshots.takeFirst();
        Q_EMIT wallpaperTransport.snapshotReceived(
            wallpaperRequest.token, wallpaperRequest.owner,
            wallpaperSnapshotWire(wallpaper, wallpaperMode));
        return QTest::qWaitFor([this] { return model.ready(); }, 5'000);
    }

    // Publishes a changed wallpaper pair through the same request/reply cycle
    // a live SettingsChanged invalidation uses.
    bool updateWallpaper(const QString &wallpaper, const QString &mode,
                         quint64 revision)
    {
        const qsizetype before = wallpaperTransport.snapshots.size();
        Q_EMIT wallpaperTransport.settingsChanged(
            QStringLiteral(":1.90"), QStringLiteral("epoch-a"), revision,
            {QStringLiteral("appearance.wallpaper")});
        if (!QTest::qWaitFor(
                [this, before] {
                    return wallpaperTransport.snapshots.size() > before;
                },
                5'000)) {
            return false;
        }
        const auto request = wallpaperTransport.snapshots.takeLast();
        Q_EMIT wallpaperTransport.snapshotReceived(
            request.token, request.owner,
            wallpaperSnapshotWire(wallpaper, mode, QStringLiteral("epoch-a"),
                                  revision));
        return true;
    }

    [[nodiscard]] QString wallpaperFilePath() const
    {
        return wallpaperStore->path() + QStringLiteral("/fixture-wall.png");
    }

    // Forwards to the window-preview theme/chrome fixture (see
    // customize_window_preview_test_support.h): publishes a confirmed theme
    // (both required Settings1 keys) and chrome-preference snapshot through
    // the same request/reply cycle a live confirmation uses. Neither client
    // needs to be established for the model itself to work: chrome stays at
    // the theme's own default until this (or nothing at all, per the
    // fail-closed contract) resolves.
    bool establishWindowPreview(const QString &themeId, const QString &colorScheme,
                                const QVariantMap &chromeOverrides,
                                quint64 chromeRevision = 7)
    {
        return windowPreviewTransports.establish(themeId, colorScheme, chromeOverrides,
                                                 chromeRevision);
    }

    // Publishes a changed theme pair through the same request/reply cycle a
    // live SettingsChanged invalidation uses.
    bool updateTheme(const QString &themeId, const QString &colorScheme,
                     quint64 revision)
    {
        return windowPreviewTransports.updateTheme(themeId, colorScheme, revision);
    }

    // Publishes a changed chrome-preference pair through the same
    // request/reply cycle a live SettingsChanged invalidation uses.
    bool updateChromePreferences(const QVariantMap &chromeOverrides,
                                 quint64 revision)
    {
        return windowPreviewTransports.updateChromePreferences(chromeOverrides, revision);
    }

    std::unique_ptr<QTemporaryDir> store;
    std::unique_ptr<QTemporaryDir> wallpaperStore;
    SequenceTransport transport;
    SequenceTransport wallpaperTransport;
    Services::SettingsClient::SettingsClient client;
    Services::SettingsClient::SettingsClient wallpaperClient;
    WindowPreviewTransports windowPreviewTransports;
    CustomizeWallpaperPreview wallpaperPreview;
    AppAppearance::ApplicationAppearanceController appearance;
    CustomizeWindowPreview windowPreview;
    MutableCustomizeOutputProvider outputProvider;
    CustomizeSettingsModel model;
};

} // namespace QindaQt::Apps::SettingsCustomize::TestSupport
