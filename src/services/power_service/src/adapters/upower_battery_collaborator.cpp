// SPDX-License-Identifier: GPL-3.0-or-later

#include <qindaqt/services/power_service/adapters/upower_battery_collaborator.h>

#include "upower_device_decoder_p.h"
#include "upstream_dbus_util.h"

#include <QtCore/QHash>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtDBus/QDBusArgument>
#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

#include <algorithm>

namespace QindaQt::Power::Upstream {
namespace {

constexpr char kService[] = "org.freedesktop.UPower";
constexpr char kRootPath[] = "/org/freedesktop/UPower";
constexpr char kRootInterface[] = "org.freedesktop.UPower";
constexpr char kDeviceInterface[] = "org.freedesktop.UPower.Device";
constexpr char kPropertiesInterface[] = "org.freedesktop.DBus.Properties";

} // namespace

struct UpowerBatteryCollaborator::RefreshCycle {
    quint64 generation = 0;
    quint64 serial = 0;
    QString owner;
    QHash<QString, UpowerDeviceTruth> devices;
    bool serviceReady = false;
    bool enumerationReady = false;
    bool onBattery = false;
    bool failed = false;
    qsizetype pendingDevices = 0;
};

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
    if (++m_nextGeneration == 0) {
        ++m_nextGeneration;
    }
    m_generation = m_nextGeneration;
    m_running = true;
    m_refresh.reset();
    m_activeOwner.clear();
    m_lastOwner.clear();
    m_epochAdvancedForLoss = false;

    if (!m_connection.isConnected()) {
        scheduleUnavailable(m_generation, QStringLiteral("upower-bus-unavailable"));
        return m_generation;
    }
    if (m_watcher == nullptr) {
        m_watcher = new QDBusServiceWatcher(
            QString::fromLatin1(kService), m_connection,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
                &UpowerBatteryCollaborator::onUpowerOwnerChanged);
    }
    if (!m_signalsSubscribed && !subscribeServiceSignals()) {
        scheduleUnavailable(m_generation, QStringLiteral("upower-unavailable"));
        return m_generation;
    }
    m_signalsSubscribed = true;
    beginRefresh();
    return m_generation;
}

void UpowerBatteryCollaborator::stop()
{
    m_running = false;
    m_refresh.reset();
    m_activeOwner.clear();
}

void UpowerBatteryCollaborator::submitSetKeyboardBrightness(
    const quint64 operationId, const Handle &device, const quint32 value)
{
    Q_UNUSED(device)
    Q_UNUSED(value)
    Q_EMIT operationFinished(
        m_generation, operationId,
        CollaboratorOutcome{.status = CollaboratorStatus::Unsupported,
                            .reasonCode =
                                QStringLiteral("keyboard-backlight-unsupported"),
                            .diagnostic = {}});
}

bool UpowerBatteryCollaborator::runningGeneration(const quint64 generation) const
{
    return m_running && generation != 0 && generation == m_generation;
}

void UpowerBatteryCollaborator::scheduleUnavailable(
    const quint64 generation, const QString &reasonCode)
{
    QTimer::singleShot(0, this, [this, generation, reasonCode]() {
        if (runningGeneration(generation)) {
            Q_EMIT statusUnavailable(generation, reasonCode);
        }
    });
}

bool UpowerBatteryCollaborator::subscribeServiceSignals()
{
    const QString service = QString::fromLatin1(kService);
    const bool added = m_connection.connect(
        service, QString::fromLatin1(kRootPath), QString::fromLatin1(kRootInterface),
        QStringLiteral("DeviceAdded"), this, SLOT(onDeviceAdded(QDBusObjectPath)));
    const bool removed = m_connection.connect(
        service, QString::fromLatin1(kRootPath), QString::fromLatin1(kRootInterface),
        QStringLiteral("DeviceRemoved"), this,
        SLOT(onDeviceRemoved(QDBusObjectPath)));
    const bool properties = m_connection.connect(
        service, QString(), QString::fromLatin1(kPropertiesInterface),
        QStringLiteral("PropertiesChanged"), this,
        SLOT(onAnyPropertiesChanged(QDBusMessage)));
    return added && removed && properties;
}

void UpowerBatteryCollaborator::beginRefresh()
{
    if (!m_running) {
        return;
    }
    if (++m_nextRefresh == 0) {
        ++m_nextRefresh;
    }
    auto cycle = std::make_shared<RefreshCycle>();
    cycle->generation = m_generation;
    cycle->serial = m_nextRefresh;
    const QDBusReply<QString> owner =
        m_connection.interface()->serviceOwner(QString::fromLatin1(kService));
    if (!owner.isValid() || owner.value().isEmpty()) {
        scheduleUnavailable(m_generation, QStringLiteral("upower-unavailable"));
        return;
    }
    cycle->owner = owner.value();
    m_refresh = cycle;
    readServiceProperties(cycle);
    enumerateDevices(cycle);
}

bool UpowerBatteryCollaborator::acceptReplyOwner(
    const std::shared_ptr<RefreshCycle> &cycle, const QString &owner)
{
    if (m_refresh != cycle || !runningGeneration(cycle->generation)
        || cycle->failed || owner.isEmpty()) {
        return false;
    }
    if (cycle->owner.isEmpty()) {
        cycle->owner = owner;
    }
    if (cycle->owner != owner) {
        failRefresh(cycle, QStringLiteral("upower-unavailable"));
        return false;
    }
    adoptOwner(owner);
    return m_activeOwner == owner;
}

void UpowerBatteryCollaborator::adoptOwner(const QString &owner)
{
    if (owner.isEmpty()) {
        return;
    }
    const bool changed = !m_lastOwner.isEmpty() && m_lastOwner != owner;
    m_activeOwner = owner;
    m_lastOwner = owner;
    if (changed && !m_epochAdvancedForLoss) {
        Q_EMIT authorityReplaced(m_generation);
    }
    m_epochAdvancedForLoss = false;
}

void UpowerBatteryCollaborator::readServiceProperties(
    const std::shared_ptr<RefreshCycle> &cycle)
{
    getAllProperties(
        m_connection, QString::fromLatin1(kService), QString::fromLatin1(kRootPath),
        QString::fromLatin1(kRootInterface), this,
        [this, cycle](const QVariantMap &properties, const QString &owner) {
            if (!acceptReplyOwner(cycle, owner)) {
                return;
            }
            bool onBattery = false;
            if (!optionalBool(properties, QStringLiteral("OnBattery"), onBattery)) {
                failRefresh(cycle, QStringLiteral("upower-malformed"));
                return;
            }
            cycle->onBattery = onBattery;
            cycle->serviceReady = true;
            tryPublish(cycle);
        },
        [this, cycle](const QString &) {
            failRefresh(cycle, QStringLiteral("upower-unavailable"));
        });
}

void UpowerBatteryCollaborator::enumerateDevices(
    const std::shared_ptr<RefreshCycle> &cycle)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        cycle->owner, QString::fromLatin1(kRootPath),
        QString::fromLatin1(kRootInterface), QStringLiteral("EnumerateDevices"));
    auto *watcher = new QDBusPendingCallWatcher(m_connection.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, cycle]() {
                const QDBusMessage reply = watcher->reply();
                watcher->deleteLater();
                if (!acceptReplyOwner(cycle, cycle->owner)) {
                    return;
                }
                if (reply.type() != QDBusMessage::ReplyMessage
                    || reply.arguments().size() != 1) {
                    failRefresh(cycle, QStringLiteral("upower-unavailable"));
                    return;
                }
                QStringList paths;
                if (!readObjectPathArray(reply.arguments().constFirst(), paths)) {
                    failRefresh(cycle, QStringLiteral("upower-malformed"));
                    return;
                }
                cycle->enumerationReady = true;
                cycle->pendingDevices = paths.size();
                for (const QString &path : paths) {
                    readDeviceProperties(cycle, path);
                }
                tryPublish(cycle);
            });
}

void UpowerBatteryCollaborator::readDeviceProperties(
    const std::shared_ptr<RefreshCycle> &cycle, const QString &objectPath)
{
    getAllProperties(
        m_connection, QString::fromLatin1(kService), objectPath,
        QString::fromLatin1(kDeviceInterface), this,
        [this, cycle, objectPath](const QVariantMap &properties,
                                  const QString &owner) {
            if (!acceptReplyOwner(cycle, owner)) {
                return;
            }
            UpowerDeviceTruth truth;
            if (!decodeUpowerDevice(objectPath, properties, truth)) {
                failRefresh(cycle, QStringLiteral("upower-malformed"));
                return;
            }
            cycle->devices.insert(objectPath, std::move(truth));
            --cycle->pendingDevices;
            tryPublish(cycle);
        },
        [this, cycle](const QString &) {
            failRefresh(cycle, QStringLiteral("upower-unavailable"));
        });
}

void UpowerBatteryCollaborator::failRefresh(
    const std::shared_ptr<RefreshCycle> &cycle, const QString &reasonCode)
{
    if (m_refresh != cycle || !runningGeneration(cycle->generation)
        || cycle->failed) {
        return;
    }
    cycle->failed = true;
    Q_EMIT statusUnavailable(cycle->generation, reasonCode);
}

void UpowerBatteryCollaborator::tryPublish(
    const std::shared_ptr<RefreshCycle> &cycle)
{
    if (m_refresh != cycle || !runningGeneration(cycle->generation)
        || cycle->failed || !cycle->serviceReady || !cycle->enumerationReady
        || cycle->pendingDevices != 0) {
        return;
    }
    BatteryFacts facts;
    facts.onBattery = cycle->onBattery;
    QStringList paths = cycle->devices.keys();
    std::sort(paths.begin(), paths.end());
    for (const QString &path : paths) {
        const UpowerDeviceTruth &truth = cycle->devices.value(path);
        facts.acPresent = facts.acPresent || truth.acPresent;
        if (truth.hasSupply) {
            facts.supplies.push_back(truth.supply);
        }
    }
    Q_EMIT factsChanged(cycle->generation, facts);
}

void UpowerBatteryCollaborator::onAnyPropertiesChanged(
    const QDBusMessage &message)
{
    if (!m_running || message.arguments().isEmpty()) {
        return;
    }
    const QString interfaceName = message.arguments().constFirst().toString();
    if ((message.path() == QString::fromLatin1(kRootPath)
         && interfaceName == QString::fromLatin1(kRootInterface))
        || interfaceName == QString::fromLatin1(kDeviceInterface)) {
        beginRefresh();
    }
}

void UpowerBatteryCollaborator::onUpowerOwnerChanged(
    const QString &name, const QString &oldOwner, const QString &newOwner)
{
    Q_UNUSED(name)
    Q_UNUSED(oldOwner)
    Q_UNUSED(newOwner)
    if (!m_running) {
        return;
    }
    m_refresh.reset();
    const QDBusReply<QString> resolved =
        m_connection.interface()->serviceOwner(QString::fromLatin1(kService));
    const QString currentOwner = resolved.isValid() ? resolved.value() : QString();
    if (currentOwner.isEmpty()) {
        if (!m_activeOwner.isEmpty()) {
            m_epochAdvancedForLoss = true;
            Q_EMIT authorityReplaced(m_generation);
        }
        m_activeOwner.clear();
        Q_EMIT statusUnavailable(m_generation,
                                 QStringLiteral("upower-unavailable"));
        return;
    }
    const bool replacement = !m_activeOwner.isEmpty()
        && m_activeOwner != currentOwner;
    adoptOwner(currentOwner);
    if (replacement) {
        Q_EMIT statusUnavailable(m_generation,
                                 QStringLiteral("upower-unavailable"));
    }
    beginRefresh();
}

void UpowerBatteryCollaborator::onDeviceAdded(const QDBusObjectPath &device)
{
    if (m_running && !device.path().isEmpty()) {
        beginRefresh();
    }
}

void UpowerBatteryCollaborator::onDeviceRemoved(const QDBusObjectPath &device)
{
    if (m_running && !device.path().isEmpty()) {
        beginRefresh();
    }
}

} // namespace QindaQt::Power::Upstream
