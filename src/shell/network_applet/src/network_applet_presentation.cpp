// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/network_applet/network_applet_presentation.h>

#include <qindaqt/services/network_model/network_model_state.h>
#include <qindaqt/services/network_protocol/network_identity.h>

#include <QtCore/QStringList>

#include <algorithm>
#include <optional>

namespace QindaQt::Shell::NetworkApplet
{
namespace
{

using Network::ActiveConnection;
using Network::DeviceKind;
using Network::KnownNetwork;
using Network::RadioKind;
using Network::SecuritySuite;
using Network::Model::ModelState;

QString kindLabel(const DeviceKind kind)
{
    switch (kind) {
    case DeviceKind::Ethernet:
        return QStringLiteral("Wired");
    case DeviceKind::Wifi:
        return QStringLiteral("Wi-Fi");
    case DeviceKind::Wwan:
        return QStringLiteral("Mobile broadband");
    }
    return QStringLiteral("Network");
}

QString radioLabel(const RadioKind kind)
{
    return kind == RadioKind::Wifi ? QStringLiteral("Wi-Fi")
                                   : QStringLiteral("Mobile broadband");
}

QString securityLabel(const SecuritySuite suite)
{
    switch (suite) {
    case SecuritySuite::Open:
        return QStringLiteral("Open");
    case SecuritySuite::Wep:
        return QStringLiteral("WEP");
    case SecuritySuite::Wpa2Personal:
        return QStringLiteral("WPA2");
    case SecuritySuite::Wpa2Enterprise:
        return QStringLiteral("WPA2 Enterprise");
    case SecuritySuite::Wpa3Personal:
        return QStringLiteral("WPA3");
    case SecuritySuite::Wpa3Enterprise:
        return QStringLiteral("WPA3 Enterprise");
    }
    return QStringLiteral("Secured");
}

QString connectivityText(const Network::ConnectivityKind kind)
{
    switch (kind) {
    case Network::ConnectivityKind::Unknown:
        return {};
    case Network::ConnectivityKind::Offline:
        return QStringLiteral("No internet access");
    case Network::ConnectivityKind::Portal:
        return QStringLiteral("Sign-in required");
    case Network::ConnectivityKind::Limited:
        return QStringLiteral("Limited connectivity");
    case Network::ConnectivityKind::Full:
        return QStringLiteral("Internet available");
    }
    return {};
}

const KnownNetwork *findKnown(const ModelState &state, const QString &id)
{
    const auto it = std::ranges::find_if(
        state.knownNetworks,
        [&id](const KnownNetwork &network) { return network.id == id; });
    return it == state.knownNetworks.cend() ? nullptr : &*it;
}

const KnownNetwork *findKnownFor(const ModelState &state,
                                 const Network::AccessPoint &point)
{
    const auto it = std::ranges::find_if(
        state.knownNetworks, [&point](const KnownNetwork &network) {
            return !network.hidden && network.ssid == point.ssid
                && network.security == point.security;
        });
    return it == state.knownNetworks.cend() ? nullptr : &*it;
}

std::optional<DeviceKind> deviceKind(const ModelState &state,
                                     const QString &interfaceName)
{
    const auto it = std::ranges::find_if(
        state.devices, [&interfaceName](const Network::Device &device) {
            return device.interfaceName == interfaceName;
        });
    return it == state.devices.cend() ? std::nullopt
                                      : std::optional<DeviceKind>(it->kind);
}

bool isActiveKnown(const ModelState &state, const QString &knownId)
{
    return std::ranges::any_of(
        state.activeConnections, [&knownId](const ActiveConnection &active) {
            return active.knownNetworkId == knownId;
        });
}

QString networkName(const KnownNetwork *known)
{
    if (known == nullptr) {
        return {};
    }
    return known->hidden || known->ssid.isEmpty()
        ? QStringLiteral("Hidden network") : known->ssid;
}

QList<RadioRow> projectRadios(const Network::Model::NetworkModel &model,
                              const ModelState &state, const bool controls)
{
    QList<RadioRow> rows;
    for (const Network::Radio &radio : state.radios) {
        if (!radio.present) {
            continue;
        }
        RadioRow row;
        row.id = radioRowId(radio.kind);
        row.kind = radio.kind;
        row.label = radioLabel(radio.kind);
        row.softwareEnabled = radio.softwareEnabled;
        row.hardwareEnabled = radio.hardwareEnabled;
        const auto verdict = model.setRadio(
            Network::SetRadioIntent{radio.kind, !radio.softwareEnabled});
        row.canToggle = controls && verdict.allowed;
        row.blockedReason = verdict.reasonCode;
        const bool on = radio.softwareEnabled && radio.hardwareEnabled;
        row.accessibleName = QStringLiteral("%1 %2").arg(
            row.label, on ? QStringLiteral("on") : QStringLiteral("off"));
        row.accessibleDescription = !radio.hardwareEnabled
            ? QStringLiteral("Turned off by a hardware switch")
            : (on ? QStringLiteral("Radio is on") : QStringLiteral("Radio is off"));
        rows.append(row);
    }
    return rows;
}

QList<ConnectionRow> projectConnections(const Network::Model::NetworkModel &model,
                                        const ModelState &state,
                                        const bool controls)
{
    QList<ConnectionRow> rows;
    for (const ActiveConnection &active : state.activeConnections) {
        const DeviceKind kind = deviceKind(state, active.deviceInterface)
                                    .value_or(DeviceKind::Ethernet);
        ConnectionRow row;
        row.id = active.deviceInterface;
        row.kind = kind;
        row.kindLabel = kindLabel(kind);
        const QString name = networkName(findKnown(state, active.knownNetworkId));
        row.label = kind == DeviceKind::Ethernet || name.isEmpty()
            ? QStringLiteral("%1 connection").arg(row.kindLabel) : name;
        row.canDisconnect = controls
            && model.disconnectDevice(Network::DisconnectIntent{active.deviceInterface})
                   .allowed;
        row.accessibleName = QStringLiteral("Connected: %1").arg(row.label);
        row.accessibleDescription = QStringLiteral("%1 on %2")
                                        .arg(row.kindLabel, active.deviceInterface);
        rows.append(row);
    }
    return rows;
}

QList<AccessPointRow> projectAccessPoints(const Network::Model::NetworkModel &model,
                                          const ModelState &state,
                                          const bool controls)
{
    QList<AccessPointRow> rows;
    for (const Network::AccessPoint &point : state.accessPoints) {
        if (point.hidden || point.ssid.isEmpty()) {
            continue;
        }
        const QString id = Network::visibleAccessPointId(point.deviceInterface,
                                                         point.bssid);
        const auto existing = std::ranges::find_if(
            rows, [&point](const AccessPointRow &row) {
                return row.label == point.ssid && row.security == point.security;
            });
        const int signal = static_cast<int>(point.signalStrength);
        if (existing != rows.end()) {
            // AGENT-NOTE: several BSSIDs of one network collapse to its
            // strongest radio; that BSSID is the connect target.
            if (signal > existing->signalPercent) {
                existing->id = id;
                existing->signalPercent = signal;
            }
            continue;
        }
        const KnownNetwork *known = findKnownFor(state, point);
        AccessPointRow row;
        row.id = id;
        row.knownNetworkId = known != nullptr ? known->id : QString();
        row.label = point.ssid;
        row.security = point.security;
        row.secured = point.security != SecuritySuite::Open;
        row.securityLabel = securityLabel(point.security);
        row.saved = known != nullptr;
        row.active = known != nullptr && isActiveKnown(state, known->id);
        row.signalPercent = signal;
        rows.append(row);
    }
    for (AccessPointRow &row : rows) {
        Network::Model::IntentVerdict verdict;
        if (row.active) {
            verdict = {false, Network::OperationKind::ConnectKnownNetwork,
                       QStringLiteral("network-already-active")};
        } else if (row.saved) {
            verdict = model.connectKnown(Network::ConnectIntent{row.knownNetworkId});
        } else {
            verdict = model.connectVisible(Network::ConnectVisibleIntent{row.id});
        }
        row.canConnect = controls && verdict.allowed;
        row.connectBlockedReason = verdict.reasonCode;
        QStringList facts;
        facts.append(QStringLiteral("signal %1%").arg(row.signalPercent));
        facts.append(row.secured ? QStringLiteral("secured, %1").arg(row.securityLabel)
                                 : QStringLiteral("open network"));
        if (row.saved) {
            facts.append(QStringLiteral("saved"));
        }
        if (row.active) {
            facts.append(QStringLiteral("connected"));
        }
        row.accessibleName = row.label;
        row.accessibleDescription = facts.join(QStringLiteral(", "));
    }
    std::ranges::stable_sort(rows, [](const AccessPointRow &a, const AccessPointRow &b) {
        if (a.signalPercent != b.signalPercent) {
            return a.signalPercent > b.signalPercent;
        }
        return a.label.localeAwareCompare(b.label) < 0;
    });
    return rows;
}

void projectIndicator(NetworkAppletModel &out, const ModelState &state)
{
    const ConnectionRow *wired = nullptr;
    const ConnectionRow *wireless = nullptr;
    const ConnectionRow *mobile = nullptr;
    for (const ConnectionRow &row : out.connections) {
        if (row.kind == DeviceKind::Ethernet && wired == nullptr) wired = &row;
        if (row.kind == DeviceKind::Wifi && wireless == nullptr) wireless = &row;
        if (row.kind == DeviceKind::Wwan && mobile == nullptr) mobile = &row;
    }
    int signal = -1;
    if (wired != nullptr) {
        out.indicator = Indicator::Wired;
        out.summaryLabel = QStringLiteral("Wired");
        out.accessibleName = QStringLiteral("Network: connected by wire");
    } else if (wireless != nullptr) {
        out.indicator = Indicator::Wireless;
        const auto active = std::ranges::find_if(
            out.accessPoints, [](const AccessPointRow &row) { return row.active; });
        if (active != out.accessPoints.cend()) {
            signal = active->signalPercent;
        }
        out.summaryLabel = QStringLiteral("Wi-Fi: %1").arg(wireless->label);
        out.accessibleName = signal >= 0
            ? QStringLiteral("Network: connected to %1 over Wi-Fi, signal %2%")
                  .arg(wireless->label).arg(signal)
            : QStringLiteral("Network: connected to %1 over Wi-Fi").arg(wireless->label);
    } else if (mobile != nullptr) {
        out.indicator = Indicator::Mobile;
        out.summaryLabel = QStringLiteral("Mobile broadband");
        out.accessibleName = QStringLiteral("Network: connected by mobile broadband");
    } else {
        const auto wifiRadio = std::ranges::find_if(
            out.radios, [](const RadioRow &row) { return row.kind == RadioKind::Wifi; });
        const bool radioOff = wifiRadio != out.radios.cend()
            && (!wifiRadio->softwareEnabled || !wifiRadio->hardwareEnabled);
        out.indicator = radioOff ? Indicator::RadioOff : Indicator::Disconnected;
        out.summaryLabel = radioOff ? QStringLiteral("Wi-Fi off")
                                    : QStringLiteral("Disconnected");
        out.accessibleName = radioOff
            ? QStringLiteral("Network: Wi-Fi is off")
            : QStringLiteral("Network: not connected");
    }
    out.iconName = indicatorIconName(out.indicator, signal, out.wifiDevicePresent);
    QStringList description;
    const QString connectivity = connectivityText(state.connectivity);
    if (!connectivity.isEmpty() && !out.connections.isEmpty()) {
        description.append(connectivity);
    }
    if (!out.diagnostic.isEmpty()) {
        description.append(out.diagnostic);
    }
    description.append(QStringLiteral("Opens network controls"));
    out.accessibleDescription = description.join(QStringLiteral(". "));
}

} // namespace

QString radioRowId(const RadioKind kind)
{
    return kind == RadioKind::Wifi ? QStringLiteral("wifi") : QStringLiteral("mobile");
}

QString indicatorIconName(const Indicator indicator, const int signalPercent,
                          const bool wifiDevicePresent)
{
    switch (indicator) {
    case Indicator::Unavailable:
        return QStringLiteral("network-offline");
    case Indicator::RadioOff:
        return QStringLiteral("network-wireless-off");
    case Indicator::Disconnected:
        return wifiDevicePresent ? QStringLiteral("network-wireless-disconnected")
                                 : QStringLiteral("network-offline");
    case Indicator::Wired:
        return QStringLiteral("network-wired");
    case Indicator::Mobile:
        return QStringLiteral("network-wireless");
    case Indicator::Wireless:
        if (signalPercent < 0) return QStringLiteral("network-wireless");
        if (signalPercent >= 75) return QStringLiteral("network-wireless-signal-excellent");
        if (signalPercent >= 50) return QStringLiteral("network-wireless-signal-good");
        if (signalPercent >= 25) return QStringLiteral("network-wireless-signal-ok");
        if (signalPercent > 0) return QStringLiteral("network-wireless-signal-weak");
        return QStringLiteral("network-wireless-signal-none");
    }
    return QStringLiteral("network-offline");
}

NetworkAppletModel projectNetworkApplet(const Network::Model::NetworkModel &model,
                                        const ProjectionContext &context)
{
    NetworkAppletModel out;
    const ModelState state = model.projection(false);
    const bool current = context.readGranted && state.hasSnapshot
        && context.snapshotCurrent
        && (context.clientPhase == ServicePhase::Ready
            || context.clientPhase == ServicePhase::Degraded);
    if (!current) {
        // AGENT-GUARD: no rows survive loss of current truth. Showing the
        // retired owner's connection as live would claim a state Network1
        // no longer vouches for.
        if (!context.readGranted) {
            out.phase = ServicePhase::Unavailable;
            out.diagnostic = QStringLiteral("Network access was not granted to this applet.");
        } else if (context.clientPhase == ServicePhase::Loading
                   || context.clientPhase == ServicePhase::Ready) {
            out.phase = ServicePhase::Loading;
            out.diagnostic = QStringLiteral("Network information is loading.");
        } else {
            out.phase = ServicePhase::Unavailable;
            out.diagnostic = QStringLiteral("The network service is unavailable.");
        }
        out.indicator = Indicator::Unavailable;
        out.iconName = indicatorIconName(Indicator::Unavailable, -1, false);
        out.summaryLabel = QStringLiteral("Network");
        out.accessibleName = out.phase == ServicePhase::Loading
            ? QStringLiteral("Network information is loading")
            : QStringLiteral("Network is unavailable");
        out.accessibleDescription = out.diagnostic;
        return out;
    }

    out.phase = context.clientPhase;
    if (out.phase == ServicePhase::Degraded) {
        out.diagnostic = !context.clientDiagnostic.isEmpty() ? context.clientDiagnostic
            : !state.diagnostic.isEmpty() ? state.diagnostic
            : QStringLiteral("Some network information is unavailable.");
    }
    out.owner = state.owner;
    out.epoch = state.epoch;
    out.revision = state.revision;
    const bool controls = context.controlGranted && context.admissionOpen;
    out.wifiDevicePresent = std::ranges::any_of(
        state.devices, [](const Network::Device &device) {
            return device.kind == DeviceKind::Wifi;
        });
    out.radios = projectRadios(model, state, controls);
    out.connections = projectConnections(model, state, controls);
    out.accessPoints = projectAccessPoints(model, state, controls);
    out.scanning = state.scanPhase == Network::ScanPhase::Scanning;
    out.scanAvailable = controls && state.scanCapable
        && model.requestScan(Network::RequestScanIntent{kScanDeadlineMilliseconds})
               .allowed;
    projectIndicator(out, state);
    return out;
}

} // namespace QindaQt::Shell::NetworkApplet
