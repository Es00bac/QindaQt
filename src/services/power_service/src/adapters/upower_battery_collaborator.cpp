// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/upower_battery_collaborator.h>

#include "upstream_dbus_util.h"
#include "upstream_identity.h"

#include <qindaqt/services/power_protocol/power_limits.h>

#include <QtCore/QTimer>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusServiceWatcher>

#include <algorithm>

namespace QindaQt::Power::Upstream {
namespace {

constexpr char kUpowerServiceName[] = "org.freedesktop.UPower";
constexpr char kUpowerObjectPath[] = "/org/freedesktop/UPower";
constexpr char kUpowerInterface[] = "org.freedesktop.UPower";
constexpr char kUpowerDeviceInterface[] = "org.freedesktop.UPower.Device";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

// Raw org.freedesktop.UPower.Device.Type values that this adapter models.
constexpr uint kUpowerTypeLinePower = 1;
constexpr uint kUpowerTypeBattery = 2;
constexpr uint kUpowerTypeUps = 3;

[[nodiscard]] bool strictUInt(const QVariantMap &properties, const QString &name,
                              uint &value)
{
    const auto it = properties.constFind(name);
    if (it == properties.constEnd() || it.value().userType() != QMetaType::UInt) {
        return false;
    }
    value = it.value().toUInt();
    return true;
}

[[nodiscard]] bool strictDouble(const QVariantMap &properties, const QString &name,
                                double &value)
{
    const auto it = properties.constFind(name);
    if (it == properties.constEnd() || it.value().userType() != QMetaType::Double) {
        return false;
    }
    value = it.value().toDouble();
    return true;
}

[[nodiscard]] bool strictInt64(const QVariantMap &properties, const QString &name,
                               qint64 &value)
{
    const auto it = properties.constFind(name);
    if (it == properties.constEnd()
        || it.value().userType() != QMetaType::LongLong) {
        return false;
    }
    value = it.value().toLongLong();
    return true;
}

[[nodiscard]] bool strictString(const QVariantMap &properties, const QString &name,
                                QString &value)
{
    const auto it = properties.constFind(name);
    if (it == properties.constEnd() || it.value().userType() != QMetaType::QString) {
        return false;
    }
    value = it.value().toString();
    return true;
}

// UPower and Power1 use the same closed ordinal sets for these enumerations,
// but the mapping stays explicit so a future upstream renumbering cannot
// silently change published truth.
[[nodiscard]] bool mapChargeState(const uint raw, ChargeState &state)
{
    switch (raw) {
    case 0: state = ChargeState::Unknown; return true;
    case 1: state = ChargeState::Charging; return true;
    case 2: state = ChargeState::Discharging; return true;
    case 3: state = ChargeState::Empty; return true;
    case 4: state = ChargeState::FullyCharged; return true;
    case 5: state = ChargeState::PendingCharge; return true;
    case 6: state = ChargeState::PendingDischarge; return true;
    default: return false;
    }
}

[[nodiscard]] bool mapBatteryLevel(const uint raw, BatteryLevel &level)
{
    switch (raw) {
    case 0: level = BatteryLevel::Unknown; return true;
    case 1: level = BatteryLevel::None; return true;
    case 2: level = BatteryLevel::Low; return true;
    case 3: level = BatteryLevel::Critical; return true;
    case 4: level = BatteryLevel::Normal; return true;
    case 5: level = BatteryLevel::High; return true;
    case 6: level = BatteryLevel::Full; return true;
    default: return false;
    }
}

[[nodiscard]] bool mapWarningLevel(const uint raw, WarningLevel &warning)
{
    switch (raw) {
    case 0: warning = WarningLevel::Unknown; return true;
    case 1: warning = WarningLevel::None; return true;
    case 2: warning = WarningLevel::Discharging; return true;
    case 3: warning = WarningLevel::Low; return true;
    case 4: warning = WarningLevel::Critical; return true;
    case 5: warning = WarningLevel::Action; return true;
    default: return false;
    }
}

} // namespace

UpowerBatteryCollaborator::UpowerBatteryCollaborator(
    const QDBusConnection &upstreamConnection, QObject *parent)
    : BatteryCollaborator(parent)
    , m_connection(upstreamConnection)
{
}

UpowerBatteryCollaborator::~UpowerBatteryCollaborator()
{
    stop();
}

quint64 UpowerBatteryCollaborator::start()
{
    ++m_nextGeneration;
    if (m_nextGeneration == 0) {
        ++m_nextGeneration;
    }
    m_generation = m_nextGeneration;
    m_running = true;
    m_onBatteryValid = false;
    m_onBatteryObserved = false;
    m_outstandingReads = 0;
    m_devices.clear();

    if (!m_connection.isConnected()) {
        scheduleUnavailable(m_generation, QStringLiteral("upower-bus-unavailable"));
        return m_generation;
    }
    if (m_watcher == nullptr) {
        m_watcher = new QDBusServiceWatcher(
            QString::fromLatin1(kUpowerServiceName), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                &UpowerBatteryCollaborator::onUpowerOwnerChanged);
    }
    if (!subscribeServiceSignals()) {
        scheduleUnavailable(m_generation, QStringLiteral("upower-unavailable"));
        return m_generation;
    }
    readServiceProperties(m_generation);
    enumerateDevices(m_generation);
    return m_generation;
}

void UpowerBatteryCollaborator::stop()
{
    // AGENT-NOTE: per-device signal hooks stay registered for this object's
    // lifetime; QtDBus drops them at destruction and every hook is
    // generation-guarded, so a stopped run can never publish again. Removing
    // an individual functor hook is not reliably supported by QtDBus.
    m_running = false;
    m_devices.clear();
    m_outstandingReads = 0;
}

void UpowerBatteryCollaborator::submitSetKeyboardBrightness(
    const quint64 operationId, const Handle &device, const quint32 value)
{
    Q_UNUSED(device)
    Q_UNUSED(value)
    Q_EMIT operationFinished(
        m_generation, operationId,
        CollaboratorOutcome{
            .status = CollaboratorStatus::Unsupported,
            .reasonCode = QStringLiteral("keyboard-backlight-unsupported"),
            .diagnostic = {}});
}

bool UpowerBatteryCollaborator::runningGeneration(const quint64 generation) const
{
    return m_running && generation != 0 && generation == m_generation;
}

void UpowerBatteryCollaborator::scheduleUnavailable(const quint64 generation,
                                                    const QString &reasonCode)
{
    QTimer::singleShot(0, this, [this, generation, reasonCode]() {
        if (!runningGeneration(generation)) {
            return;
        }
        Q_EMIT statusUnavailable(generation, reasonCode);
    });
}

bool UpowerBatteryCollaborator::subscribeServiceSignals()
{
    const QString service = QString::fromLatin1(kUpowerServiceName);
    const QString path = QString::fromLatin1(kUpowerObjectPath);
    const QString interface = QString::fromLatin1(kUpowerInterface);
    const bool added = m_connection.connect(
        service, path, interface, QStringLiteral("DeviceAdded"), this,
        SLOT(onDeviceAdded(QDBusObjectPath)));
    const bool removed = m_connection.connect(
        service, path, interface, QStringLiteral("DeviceRemoved"), this,
        SLOT(onDeviceRemoved(QDBusObjectPath)));
    // One wildcard-path subscription covers the service object and every
    // device object; the QDBusMessage slot form carries the emitting path.
    const bool propertiesChanged = m_connection.connect(
        service, QString(), QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("PropertiesChanged"), this,
        SLOT(onAnyPropertiesChanged(QDBusMessage)));
    return added && removed && propertiesChanged;
}

void UpowerBatteryCollaborator::readServiceProperties(const quint64 generation)
{
    Upstream::getAllProperties(
        m_connection, QString::fromLatin1(kUpowerServiceName),
        QString::fromLatin1(kUpowerObjectPath),
        QString::fromLatin1(kUpowerInterface), this,
        [this, generation](const QVariantMap &properties, const QString &) {
            if (!runningGeneration(generation)) {
                return;
            }
            applyServiceProperties(generation, properties);
        },
        [this, generation](const QString &) {
            if (!runningGeneration(generation)) {
                return;
            }
            scheduleUnavailable(generation, QStringLiteral("upower-unavailable"));
        });
}

void UpowerBatteryCollaborator::applyServiceProperties(
    const quint64 generation, const QVariantMap &properties)
{
    bool onBattery = false;
    // OnBattery is the AC/on-battery authority. Its absence leaves prior truth
    // untouched rather than guessing "on AC".
    if (Upstream::optionalBool(properties, QStringLiteral("OnBattery"), onBattery)) {
        m_onBatteryValid = true;
        m_onBatteryObserved = onBattery;
    }
    if (m_outstandingReads == 0) {
        publishFacts(generation);
    }
}

void UpowerBatteryCollaborator::enumerateDevices(const quint64 generation)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(kUpowerServiceName),
        QString::fromLatin1(kUpowerObjectPath),
        QString::fromLatin1(kUpowerInterface), QStringLiteral("EnumerateDevices"));
    auto *watcher =
        new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, generation]() {
                watcher->deleteLater();
                if (!runningGeneration(generation)) {
                    return;
                }
                const QDBusMessage reply = watcher->reply();
                if (reply.type() != QDBusMessage::ReplyMessage
                    || reply.arguments().isEmpty()) {
                    scheduleUnavailable(generation,
                                        QStringLiteral("upower-unavailable"));
                    return;
                }
                QStringList paths;
                if (!Upstream::readObjectPathArray(reply.arguments().constFirst(),
                                                   paths)) {
                    scheduleUnavailable(generation,
                                        QStringLiteral("upower-malformed"));
                    return;
                }
                if (paths.isEmpty()) {
                    publishFacts(generation);
                    return;
                }
                for (const QString &path : paths) {
                    readDeviceProperties(generation, path);
                }
            });
}

void UpowerBatteryCollaborator::readDeviceProperties(const quint64 generation,
                                                     const QString &objectPath)
{
    ++m_outstandingReads;
    Upstream::getAllProperties(
        m_connection, QString::fromLatin1(kUpowerServiceName), objectPath,
        QString::fromLatin1(kUpowerDeviceInterface), this,
        [this, generation, objectPath](const QVariantMap &properties,
                                       const QString &) {
            if (!runningGeneration(generation)) {
                return;
            }
            const bool wellFormed =
                applyDeviceProperties(generation, objectPath, properties);
            --m_outstandingReads;
            if (!wellFormed) {
                scheduleUnavailable(generation,
                                    QStringLiteral("upower-malformed"));
                return;
            }
            if (m_outstandingReads == 0) {
                publishFacts(generation);
            }
        },
        [this, generation](const QString &) {
            if (!runningGeneration(generation)) {
                return;
            }
            --m_outstandingReads;
            scheduleUnavailable(generation, QStringLiteral("upower-unavailable"));
        });
}

bool UpowerBatteryCollaborator::applyDeviceProperties(
    const quint64 generation, const QString &objectPath,
    const QVariantMap &properties)
{
    Q_UNUSED(generation)
    uint type = 0;
    if (!strictUInt(properties, QStringLiteral("Type"), type)) {
        return false;
    }
    DeviceTruth truth;
    truth.upstreamType = type;

    if (type != kUpowerTypeLinePower && type != kUpowerTypeBattery
        && type != kUpowerTypeUps) {
        m_devices.insert(objectPath, truth);
        return true;
    }

    bool isPresent = true;
    if (!Upstream::optionalBool(properties, QStringLiteral("IsPresent"),
                                isPresent)) {
        isPresent = true;
    }

    if (type == kUpowerTypeLinePower) {
        truth.acPresent = isPresent;
        m_devices.insert(objectPath, truth);
        return true;
    }

    PowerSupply supply;
    supply.handle.opaqueId = Upstream::deriveOpaqueId(
        QStringLiteral("upower-supply"), objectPath);
    supply.kind =
        type == kUpowerTypeBattery ? SupplyKind::Battery : SupplyKind::Ups;
    supply.present = isPresent;
    // Present-but-wrongly-typed optional text is hostile input, not absence.
    if (properties.contains(QStringLiteral("Vendor"))
        && !strictString(properties, QStringLiteral("Vendor"), supply.vendor)) {
        return false;
    }
    if (properties.contains(QStringLiteral("Model"))
        && !strictString(properties, QStringLiteral("Model"), supply.model)) {
        return false;
    }

    uint rawState = 0;
    if (strictUInt(properties, QStringLiteral("State"), rawState)) {
        if (!mapChargeState(rawState, supply.state)) {
            return false;
        }
    }
    uint rawWarning = 0;
    if (strictUInt(properties, QStringLiteral("WarningLevel"), rawWarning)) {
        if (!mapWarningLevel(rawWarning, supply.warning)) {
            return false;
        }
    }
    uint rawLevel = 0;
    const bool hasLevel =
        strictUInt(properties, QStringLiteral("BatteryLevel"), rawLevel);
    BatteryLevel mappedLevel = BatteryLevel::Unknown;
    if (hasLevel && !mapBatteryLevel(rawLevel, mappedLevel)) {
        return false;
    }

    double percentage = 0.0;
    const bool hasPercentage =
        strictDouble(properties, QStringLiteral("Percentage"), percentage);
    // Coarse upstream truth is retained only when exact percentage truth is
    // absent, mirroring the Power1 known-flag contract.
    const bool exactPercentage =
        hasPercentage && (!hasLevel || rawLevel == 0 || rawLevel == 1);
    if (exactPercentage) {
        supply.percentageKnown = true;
        supply.percentage = percentage;
        supply.level = BatteryLevel::None;
    } else if (hasLevel) {
        supply.level = mappedLevel;
    }

    double energy = 0.0;
    double energyFull = 0.0;
    const bool hasEnergy =
        strictDouble(properties, QStringLiteral("Energy"), energy);
    const bool hasEnergyFull =
        strictDouble(properties, QStringLiteral("EnergyFull"), energyFull);
    if (hasEnergy && hasEnergyFull) {
        supply.energyKnown = true;
        supply.energyWattHours = energy;
        supply.energyFullWattHours = energyFull;
    }
    double rate = 0.0;
    if (strictDouble(properties, QStringLiteral("EnergyRate"), rate)) {
        supply.rateKnown = true;
        supply.energyRateWatts = rate;
    }
    qint64 timeToEmpty = 0;
    if (strictInt64(properties, QStringLiteral("TimeToEmpty"), timeToEmpty)) {
        supply.timeToEmptyKnown = true;
        supply.timeToEmptySeconds = timeToEmpty;
    }
    qint64 timeToFull = 0;
    if (strictInt64(properties, QStringLiteral("TimeToFull"), timeToFull)) {
        supply.timeToFullKnown = true;
        supply.timeToFullSeconds = timeToFull;
    }

    truth.hasSupply = true;
    truth.supply = std::move(supply);
    m_devices.insert(objectPath, truth);
    // Per-device PropertiesChanged arrives through the wildcard-path slot;
    // membership in m_devices is the dispatch table.
    return true;
}

void UpowerBatteryCollaborator::onAnyPropertiesChanged(const QDBusMessage &message)
{
    if (!m_running || message.arguments().size() < 2) {
        return;
    }
    const QString interfaceName = message.arguments().at(0).toString();
    const QString path = message.path();
    if (path == QString::fromLatin1(kUpowerObjectPath)) {
        if (interfaceName != QString::fromLatin1(kUpowerInterface)) {
            return;
        }
        const QVariantMap changed = message.arguments().at(1).toMap();
        bool onBattery = false;
        if (Upstream::optionalBool(changed, QStringLiteral("OnBattery"),
                                   onBattery)) {
            m_onBatteryValid = true;
            m_onBatteryObserved = onBattery;
            publishFacts(m_generation);
        }
        return;
    }
    if (interfaceName == QString::fromLatin1(kUpowerDeviceInterface)
        && m_devices.contains(path)) {
        // The per-device property map is the only truth; re-read it wholesale
        // instead of trusting inline changed values.
        readDeviceProperties(m_generation, path);
    }
}

void UpowerBatteryCollaborator::publishFacts(const quint64 generation)
{
    if (!runningGeneration(generation)) {
        return;
    }
    BatteryFacts facts;
    facts.onBattery = m_onBatteryValid && m_onBatteryObserved;
    QStringList paths = m_devices.keys();
    std::sort(paths.begin(), paths.end());
    for (const QString &path : paths) {
        const DeviceTruth &truth = m_devices.value(path);
        if (truth.acPresent) {
            facts.acPresent = true;
        }
        if (truth.hasSupply) {
            facts.supplies.push_back(truth.supply);
        }
    }
    Q_EMIT factsChanged(generation, facts);
}

void UpowerBatteryCollaborator::onUpowerOwnerChanged(const QString &name,
                                                     const QString &oldOwner,
                                                     const QString &newOwner)
{
    Q_UNUSED(name)
    if (!m_running) {
        return;
    }
    if (newOwner.isEmpty()) {
        m_devices.clear();
        scheduleUnavailable(m_generation, QStringLiteral("upower-unavailable"));
        return;
    }
    if (!oldOwner.isEmpty() && oldOwner != newOwner) {
        // AGENT-GUARD: a replaced bus authority invalidates every published
        // handle; the coordinator advances the epoch on this signal and any
        // stale-handle operation completes as Uncertain exactly once.
        Q_EMIT authorityReplaced(m_generation);
    }
    m_devices.clear();
    readServiceProperties(m_generation);
    enumerateDevices(m_generation);
}

void UpowerBatteryCollaborator::onDeviceAdded(const QDBusObjectPath &device)
{
    if (!m_running || device.path().isEmpty()) {
        return;
    }
    readDeviceProperties(m_generation, device.path());
}

void UpowerBatteryCollaborator::onDeviceRemoved(const QDBusObjectPath &device)
{
    if (!m_running) {
        return;
    }
    if (m_devices.remove(device.path()) > 0) {
        publishFacts(m_generation);
    }
}

} // namespace QindaQt::Power::Upstream
