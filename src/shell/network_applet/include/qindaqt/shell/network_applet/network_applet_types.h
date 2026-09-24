// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/network_protocol/network_types.h>

#include <QtCore/QList>
#include <QtCore/QString>

namespace QindaQt::Shell::NetworkApplet
{

// A Rescan asks Network1 to keep fresh results for at most this long; the
// same bounded deadline the Settings route uses.
inline constexpr qint64 kScanDeadlineMilliseconds = 30'000;

// Presentation phase of the one bound Network1 owner. Only Ready and Degraded
// carry rows; every other phase publishes no inventory at all, so last-known
// truth from a retired or absent owner is never shown as current.
enum class ServicePhase {
    Loading,
    Ready,
    Degraded,
    Unavailable,
};

// What the panel glyph claims. Each value has a distinct accessible name so
// the state is never conveyed by the icon alone.
enum class Indicator {
    Unavailable,
    RadioOff,
    Disconnected,
    Wired,
    Wireless,
    Mobile,
};

struct RadioRow {
    // Stable QML id: "wifi" or "mobile". Only radios Network1 reports as
    // present are projected (ADR-0251).
    QString id;
    Network::RadioKind kind = Network::RadioKind::Wifi;
    QString label;
    bool softwareEnabled = false;
    bool hardwareEnabled = false;
    bool canToggle = false;
    QString blockedReason;
    QString accessibleName;
    QString accessibleDescription;

    friend bool operator==(const RadioRow &, const RadioRow &) = default;
};

struct ConnectionRow {
    // The normalized device interface; this is also the Disconnect target.
    QString id;
    Network::DeviceKind kind = Network::DeviceKind::Ethernet;
    QString label;
    QString kindLabel;
    bool canDisconnect = false;
    QString accessibleName;
    QString accessibleDescription;

    friend bool operator==(const ConnectionRow &, const ConnectionRow &) = default;
};

// One row per visible (SSID, security) pair: the strongest access point of
// that network. Hidden networks are omitted because Network1 cannot join them
// from a visible row.
struct AccessPointRow {
    // Opaque Network1 visible-access-point id of the strongest BSSID.
    QString id;
    // Non-empty when a saved profile matches; connecting then uses
    // connectKnownNetwork with this id instead of creating a profile.
    QString knownNetworkId;
    QString label;
    Network::SecuritySuite security = Network::SecuritySuite::Open;
    QString securityLabel;
    bool secured = false;
    bool saved = false;
    bool active = false;
    int signalPercent = 0;
    bool canConnect = false;
    QString connectBlockedReason;
    QString accessibleName;
    QString accessibleDescription;

    friend bool operator==(const AccessPointRow &, const AccessPointRow &) = default;
};

struct NetworkAppletModel {
    ServicePhase phase = ServicePhase::Unavailable;
    QString diagnostic;
    QString owner;
    quint64 epoch = 0;
    quint64 revision = 0;
    Indicator indicator = Indicator::Unavailable;
    QString iconName;
    QString summaryLabel;
    QString accessibleName;
    QString accessibleDescription;
    bool wifiDevicePresent = false;
    bool scanAvailable = false;
    bool scanning = false;
    QList<RadioRow> radios;
    QList<ConnectionRow> connections;
    QList<AccessPointRow> accessPoints;

    friend bool operator==(const NetworkAppletModel &,
                           const NetworkAppletModel &) = default;
};

[[nodiscard]] QString radioRowId(Network::RadioKind kind);

// AGENT-CONTRACT: The icon vocabulary is closed. Every name returned here
// must exist in the QindaQt icon theme and in the pinned Breeze fixture;
// tests/shell/verify_shell_icon_coverage.cmake enforces both.
// `signalPercent` is negative when the active access point's strength is
// unknown; that selects the plain Wi-Fi glyph rather than an invented bar.
[[nodiscard]] QString indicatorIconName(Indicator indicator,
                                        int signalPercent,
                                        bool wifiDevicePresent);

} // namespace QindaQt::Shell::NetworkApplet
