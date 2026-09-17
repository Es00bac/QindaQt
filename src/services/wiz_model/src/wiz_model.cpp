// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/services/wiz_model/wiz_model.h"

#include "qindaqt/services/wiz_protocol/wiz_capabilities.h"
#include "qindaqt/services/wiz_protocol/wiz_limits.h"

#include <algorithm>
#include <limits>

namespace QindaQt::Wiz
{

WizModel::WizModel() = default;

void WizModel::start()
{
    // A new epoch invalidates every prior observation. Stored labels are ours,
    // not the device's, so they survive; inventory does not.
    ++m_epoch;
    m_revision = 0;
    m_running = true;
    m_discovering = false;
    m_availability = Availability::Starting;
    m_reasonCode.clear();
    m_diagnostic.clear();
    m_devices.clear();
    m_modelConfigs.clear();
}

void WizModel::stop()
{
    m_running = false;
    m_discovering = false;
    m_availability = Availability::Unavailable;
    m_reasonCode = QStringLiteral("client-stopped");
    m_devices.clear();
    m_modelConfigs.clear();
    ++m_revision;
}

Snapshot WizModel::snapshot() const
{
    Snapshot snapshot;
    snapshot.epoch = m_epoch;
    snapshot.revision = m_revision;
    snapshot.availability = m_availability;
    snapshot.discovering = m_discovering;
    snapshot.reasonCode = m_reasonCode;
    snapshot.diagnostic = m_diagnostic;
    snapshot.devices = m_devices.values();
    // Deterministic order: what the user reads, then a stable tiebreak. Two
    // lights may legitimately share a label.
    std::sort(snapshot.devices.begin(), snapshot.devices.end(),
              [](const Device &left, const Device &right) {
                  const int byLabel =
                      QString::compare(left.label, right.label, Qt::CaseInsensitive);
                  if (byLabel != 0) {
                      return byLabel < 0;
                  }
                  return left.identity.mac < right.identity.mac;
              });
    return snapshot;
}

std::optional<Device> WizModel::device(const QString &mac) const
{
    const auto it = m_devices.constFind(mac);
    if (it == m_devices.cend()) {
        return std::nullopt;
    }
    return *it;
}

QStringList WizModel::knownMacs() const
{
    QStringList macs = m_devices.keys();
    std::sort(macs.begin(), macs.end());
    return macs;
}

std::optional<DeviceIdentity> WizModel::endpoint(const QString &mac) const
{
    const auto it = m_devices.constFind(mac);
    if (it == m_devices.cend() || it->identity.address.isEmpty()) {
        return std::nullopt;
    }
    return it->identity;
}

void WizModel::refreshCapabilities(Device &device) const
{
    const auto configIt = m_modelConfigs.constFind(device.identity.mac);
    const std::optional<ModelConfigPayload> config =
        configIt == m_modelConfigs.cend() ? std::nullopt
                                          : std::optional<ModelConfigPayload>(*configIt);
    const CapabilityProfile profile =
        inferCapabilities(device.identity.moduleName, config);
    device.features = profile.features;
    device.dimming = profile.dimming;
    device.temperature = profile.temperature;
    device.capabilitiesKnown =
        profile.complete && !device.identity.moduleName.isEmpty();
}

bool WizModel::commit(const QString &mac, const Device &updated)
{
    const auto it = m_devices.find(mac);
    if (it != m_devices.end() && *it == updated) {
        return false;
    }
    m_devices.insert(mac, updated);
    ++m_revision;
    return true;
}

bool WizModel::observe(const QString &address, const quint16 port,
                       const DecodedMessage &message)
{
    if (!m_running) {
        return false;
    }
    // Identity is the device's own MAC, because a source address is not an
    // identity: DHCP moves it, and any peer can claim it.
    //
    // AGENT-NOTE (verified on firmware 1.38.0): getModelConfig answers without
    // a `mac` member, unlike getPilot and getSystemConfig. Such a reply is
    // attributed to the one device already known at that exact address, and
    // never admits a new one. Without this, a luminaire's capabilities could
    // never complete and every capability-dependent control would stay inert.
    QString mac = normalizeMac(message.mac);
    if (mac.isEmpty()) {
        mac = soleDeviceAt(address);
    }
    if (mac.isEmpty()) {
        return false;
    }
    if (!m_devices.contains(mac) && m_devices.size() >= Limits::maximumDevices) {
        return false;
    }

    Device device = m_devices.value(mac);
    device.identity.mac = mac;
    if (!address.isEmpty()) {
        device.identity.address = address;
    }
    if (port != 0) {
        device.identity.port = port;
    }

    if (message.systemConfigKnown) {
        device.identity.moduleName = message.systemConfig.moduleName;
        device.identity.firmwareVersion = message.systemConfig.firmwareVersion;
        device.identity.homeId = message.systemConfig.homeId;
        device.identity.roomId = message.systemConfig.roomId;
    }
    if (message.modelConfigKnown) {
        m_modelConfigs.insert(mac, message.modelConfig);
    }
    if (message.pilotKnown) {
        device.pilot = message.pilot;
        device.pilotKnown = true;
    }

    // Any answer at all proves the light is reachable, including an error
    // reply: the device is there, it just refused this request.
    device.reachability = Reachability::Online;
    device.missedPolls = 0;

    refreshCapabilities(device);

    const QString stored = m_storedLabels.value(mac);
    device.labelStored = !stored.isEmpty();
    device.label = device.labelStored ? stored : derivedLabel(device.identity);

    return commit(mac, device);
}

QString WizModel::deviceAtAddress(const QString &address) const
{
    return soleDeviceAt(address);
}

QString WizModel::soleDeviceAt(const QString &address) const
{
    if (address.isEmpty()) {
        return {};
    }
    QString found;
    for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
        if (it->identity.address != address) {
            continue;
        }
        if (!found.isEmpty()) {
            // Two devices claiming one address is not a fact to guess from.
            return {};
        }
        found = it.key();
    }
    return found;
}

bool WizModel::noteMissedPoll(const QString &mac)
{
    const auto it = m_devices.constFind(mac);
    if (it == m_devices.cend()) {
        return false;
    }
    Device device = *it;
    if (device.missedPolls < std::numeric_limits<quint32>::max()) {
        ++device.missedPolls;
    }
    if (device.missedPolls >= unreachableAfterMissedPolls) {
        device.reachability = Reachability::Unreachable;
        // A light that is not answering is not proof of its last pilot.
        device.pilotKnown = false;
    } else if (device.missedPolls >= staleAfterMissedPolls) {
        device.reachability = Reachability::Stale;
    }
    return commit(mac, device);
}

bool WizModel::applyStoredLabel(const QString &mac, const QString &label)
{
    const QString normalized = normalizeMac(mac);
    if (normalized.isEmpty()) {
        return false;
    }
    const QString trimmed = label.trimmed().left(Limits::maximumLabelLength);
    if (trimmed.isEmpty()) {
        m_storedLabels.remove(normalized);
    } else {
        m_storedLabels.insert(normalized, trimmed);
    }
    const auto it = m_devices.constFind(normalized);
    if (it == m_devices.cend()) {
        return false;
    }
    Device device = *it;
    device.labelStored = !trimmed.isEmpty();
    device.label = device.labelStored ? trimmed : derivedLabel(device.identity);
    return commit(normalized, device);
}

bool WizModel::setDiscovering(const bool discovering)
{
    if (m_discovering == discovering) {
        return false;
    }
    m_discovering = discovering;
    ++m_revision;
    return true;
}

bool WizModel::setAvailability(const Availability availability,
                               const QString &reasonCode, const QString &diagnostic)
{
    if (m_availability == availability && m_reasonCode == reasonCode
        && m_diagnostic == diagnostic) {
        return false;
    }
    m_availability = availability;
    m_reasonCode = reasonCode;
    m_diagnostic = diagnostic;
    ++m_revision;
    return true;
}

bool WizModel::forget(const QString &mac)
{
    const QString normalized = normalizeMac(mac);
    if (normalized.isEmpty() || m_devices.remove(normalized) == 0) {
        return false;
    }
    m_modelConfigs.remove(normalized);
    m_storedLabels.remove(normalized);
    ++m_revision;
    return true;
}

} // namespace QindaQt::Wiz
