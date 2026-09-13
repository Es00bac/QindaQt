// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "customize_sequence_transport.h"

#include "qindaqt/decoration_painter/decoration_painter.h"
#include "qindaqt/services/settings_client/settings_client.h"
#include "qindaqt/services/settings_protocol/settings_wire_contract.h"
#include "qindaqt/services/settings_protocol/settings_wire_status.h"

#include <QtTest>

namespace QindaQt::Apps::SettingsCustomize::TestSupport {

inline QStringList themeDirectories()
{
    return {QStringLiteral(QINDAQT_SOURCE_DIR "/data/themes")};
}

// A generic Settings1 confirmed-snapshot wire, unlike snapshotWire()/
// wallpaperSnapshotWire(): the caller supplies the full key/value set, so one
// helper serves both the theme scope (appearance.theme + appearance.
// colorScheme) and the chrome-preference scope.
inline QVariantMap valuesSnapshotWire(
    const QVariantMap &values,
    const QString &epoch = QStringLiteral("epoch-a"),
    quint64 revision = 7)
{
    using Services::SettingsProtocol::SettingsWireStatus;
    using Services::SettingsProtocol::WireContract;
    QVariantMap sources;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        sources.insert(it.key(), QStringLiteral("user-overrides"));
    }
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

// Owns the two independently-scoped Settings1 transports/clients the
// contained-window preview needs (theme: appearance.theme + appearance.
// colorScheme, matching production's ApplicationAppearanceController scope;
// chrome: Decoration::ChromePreferences::settingsKeys()), plus the
// request/reply helpers a test uses to simulate a confirmed snapshot or a
// changed revision. Deliberately independent of ModelHarness's panel/profile
// scope so a window-preview test can be reasoned about on its own.
class WindowPreviewTransports final {
public:
    WindowPreviewTransports()
        : themeClient(themeTransport,
                      {QStringLiteral("appearance.theme"),
                       QStringLiteral("appearance.colorScheme")},
                      {.requestTimeoutMilliseconds = 100,
                       .debounceMilliseconds = 0,
                       .retryMilliseconds = {10}})
        , chromeClient(chromeTransport,
                       Decoration::ChromePreferences::settingsKeys(),
                       {.requestTimeoutMilliseconds = 100,
                        .debounceMilliseconds = 0,
                        .retryMilliseconds = {10}})
    {
    }

    // The chrome client is scoped to the exact ChromePreferences::settingsKeys()
    // set (matching production wiring): a wire snapshot must carry every one
    // of those keys or SettingsClient rejects it as out of scope. Overrides
    // fill in on top of the "theme" defaults for every other key.
    [[nodiscard]] static QVariantMap fullChromeValues(const QVariantMap &overrides)
    {
        return Decoration::ChromePreferences::fromSettingsValues(overrides)
            .toSettingsValues();
    }

    // Publishes a confirmed theme (both required keys) and chrome-preference
    // snapshot through the same request/reply cycle a live Settings1
    // confirmation uses. Neither client needs to be established for the
    // model itself to work: chrome stays at the theme's own default until
    // this (or nothing at all, per the fail-closed contract) resolves.
    bool establish(const QString &themeId, const QString &colorScheme,
                   const QVariantMap &chromeOverrides, quint64 chromeRevision = 7)
    {
        using Services::SettingsClient::ClientState;
        if (!themeClient.start() || !chromeClient.start()) {
            return false;
        }
        Q_EMIT themeTransport.ownerChanged(QStringLiteral(":1.90"));
        Q_EMIT chromeTransport.ownerChanged(QStringLiteral(":1.90"));
        if (!QTest::qWaitFor(
                [this] { return !themeTransport.snapshots.isEmpty(); }, 5'000)
            || !QTest::qWaitFor(
                [this] { return !chromeTransport.snapshots.isEmpty(); },
                5'000)) {
            return false;
        }
        const auto themeRequest = themeTransport.snapshots.takeFirst();
        Q_EMIT themeTransport.snapshotReceived(
            themeRequest.token, themeRequest.owner,
            valuesSnapshotWire({{QStringLiteral("appearance.theme"), themeId},
                               {QStringLiteral("appearance.colorScheme"), colorScheme}}));
        const auto chromeRequest = chromeTransport.snapshots.takeFirst();
        Q_EMIT chromeTransport.snapshotReceived(
            chromeRequest.token, chromeRequest.owner,
            valuesSnapshotWire(fullChromeValues(chromeOverrides),
                               QStringLiteral("epoch-a"), chromeRevision));
        return QTest::qWaitFor(
            [this] {
                return themeClient.state() == ClientState::Ready
                    && chromeClient.state() == ClientState::Ready;
            },
            5'000);
    }

    // Publishes a changed theme pair through the same request/reply cycle a
    // live SettingsChanged invalidation uses.
    bool updateTheme(const QString &themeId, const QString &colorScheme,
                     quint64 revision)
    {
        const qsizetype before = themeTransport.snapshots.size();
        Q_EMIT themeTransport.settingsChanged(
            QStringLiteral(":1.90"), QStringLiteral("epoch-a"), revision,
            {QStringLiteral("appearance.theme"), QStringLiteral("appearance.colorScheme")});
        if (!QTest::qWaitFor(
                [this, before] { return themeTransport.snapshots.size() > before; },
                5'000)) {
            return false;
        }
        const auto request = themeTransport.snapshots.takeLast();
        Q_EMIT themeTransport.snapshotReceived(
            request.token, request.owner,
            valuesSnapshotWire({{QStringLiteral("appearance.theme"), themeId},
                               {QStringLiteral("appearance.colorScheme"), colorScheme}},
                               QStringLiteral("epoch-a"), revision));
        return true;
    }

    // Publishes a changed chrome-preference pair through the same
    // request/reply cycle a live SettingsChanged invalidation uses.
    bool updateChromePreferences(const QVariantMap &chromeOverrides,
                                 quint64 revision)
    {
        const qsizetype before = chromeTransport.snapshots.size();
        Q_EMIT chromeTransport.settingsChanged(
            QStringLiteral(":1.90"), QStringLiteral("epoch-a"), revision,
            chromeOverrides.keys());
        if (!QTest::qWaitFor(
                [this, before] {
                    return chromeTransport.snapshots.size() > before;
                },
                5'000)) {
            return false;
        }
        const auto request = chromeTransport.snapshots.takeLast();
        Q_EMIT chromeTransport.snapshotReceived(
            request.token, request.owner,
            valuesSnapshotWire(fullChromeValues(chromeOverrides),
                               QStringLiteral("epoch-a"), revision));
        return true;
    }

    SequenceTransport themeTransport;
    SequenceTransport chromeTransport;
    Services::SettingsClient::SettingsClient themeClient;
    Services::SettingsClient::SettingsClient chromeClient;
};

} // namespace QindaQt::Apps::SettingsCustomize::TestSupport
