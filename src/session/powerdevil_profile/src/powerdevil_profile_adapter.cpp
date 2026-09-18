// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/session/powerdevil_profile/powerdevil_profile_adapter.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

#include <utility>

namespace QindaQt::Session::PowerDevilProfile {
namespace {

constexpr auto PowerDevilService = "org.kde.Solid.PowerManagement";
constexpr auto PowerDevilPath = "/org/kde/Solid/PowerManagement";
constexpr auto PowerDevilInterface = "org.kde.Solid.PowerManagement";
constexpr auto PowerDevilConfig = "powerdevilrc";
constexpr auto PerformanceGroup = "Performance";
constexpr auto PowerProfileKey = "PowerProfile";
constexpr auto AcProfile = "AC";
constexpr auto BatteryProfile = "Battery";
constexpr auto LowBatteryProfile = "LowBattery";

QString ownerFor(const QDBusConnection &bus)
{
    if (!bus.isConnected() || bus.interface() == nullptr) {
        return {};
    }
    const QDBusReply<QString> owner =
        bus.interface()->serviceOwner(QString::fromLatin1(PowerDevilService));
    return owner.isValid() ? owner.value() : QString{};
}

QString dbusError(const QDBusError &error)
{
    return QStringLiteral("powerdevil-refresh-failed:%1")
        .arg(error.message().isEmpty() ? QStringLiteral("unknown-error")
                                        : error.message());
}

QString readProfile(const KConfigGroup &performance)
{
    // AGENT-GUARD: an absent key reads as empty ("no automatic switch"),
    // never a guessed default -- this adapter must never fabricate a
    // profile choice the user never made.
    return performance.readEntry(QString::fromLatin1(PowerProfileKey), QString());
}

} // namespace

PowerDevilProfileAdapter::PowerDevilProfileAdapter(QDBusConnection sessionBus,
                                                    QObject *parent)
    : QObject(parent)
    , m_sessionBus(std::move(sessionBus))
{
}

PowerDevilProfileAdapter::~PowerDevilProfileAdapter()
{
    stop();
}

void PowerDevilProfileAdapter::start()
{
    if (m_serviceWatcher != nullptr) {
        return;
    }
    if (m_sessionBus.isConnected()) {
        m_serviceWatcher = new QDBusServiceWatcher(
            QString::fromLatin1(PowerDevilService), m_sessionBus,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
                this, &PowerDevilProfileAdapter::ownerChanged);
    }
    refreshAvailability();
}

void PowerDevilProfileAdapter::stop()
{
    ++m_requestSerial;
    if (m_applying) {
        m_applying = false;
        Q_EMIT applyingChanged();
    }
    delete m_serviceWatcher;
    m_serviceWatcher = nullptr;
    setAvailable(false);
}

bool PowerDevilProfileAdapter::available() const noexcept
{
    return m_available;
}

bool PowerDevilProfileAdapter::applying() const noexcept
{
    return m_applying;
}

const QString &PowerDevilProfileAdapter::acProfileId() const noexcept
{
    return m_acProfileId;
}

const QString &PowerDevilProfileAdapter::batteryProfileId() const noexcept
{
    return m_batteryProfileId;
}

const QString &PowerDevilProfileAdapter::lowBatteryProfileId() const noexcept
{
    return m_lowBatteryProfileId;
}

const QString &PowerDevilProfileAdapter::error() const noexcept
{
    return m_error;
}

bool PowerDevilProfileAdapter::apply(const QString &acProfileId,
                                     const QString &batteryProfileId,
                                     const QString &lowBatteryProfileId)
{
    if (m_applying) {
        setError(QStringLiteral("powerdevil-apply-busy"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }
    if (acProfileId.contains(QLatin1Char('\n'))
        || batteryProfileId.contains(QLatin1Char('\n'))
        || lowBatteryProfileId.contains(QLatin1Char('\n'))) {
        setError(QStringLiteral("power-profile-id-invalid"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }
    if (!m_available) {
        setError(QStringLiteral("powerdevil-unavailable"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }

    QString writeError;
    if (!writePreferences(acProfileId, batteryProfileId, lowBatteryProfileId,
                          &writeError)) {
        setError(writeError);
        Q_EMIT applyFinished(false, m_error);
        return false;
    }

    setError({});
    m_acProfileId = acProfileId;
    m_batteryProfileId = batteryProfileId;
    m_lowBatteryProfileId = lowBatteryProfileId;
    Q_EMIT preferencesChanged();
    m_applying = true;
    Q_EMIT applyingChanged();
    requestRefresh(false);
    return true;
}

void PowerDevilProfileAdapter::refreshAvailability()
{
    const bool available = !ownerFor(m_sessionBus).isEmpty();
    if (available) {
        // A newly seen owner may have had its config changed by another
        // writer (the Energy Saving KCM, e.g.) since the last read.
        readPreferences();
    }
    setAvailable(available);
}

void PowerDevilProfileAdapter::readPreferences()
{
    const KSharedConfig::Ptr config =
        KSharedConfig::openConfig(QString::fromLatin1(PowerDevilConfig));
    if (!config) {
        return;
    }
    config->reparseConfiguration();
    const QString storedAc = readProfile(
        config->group(QString::fromLatin1(AcProfile))
            .group(QString::fromLatin1(PerformanceGroup)));
    const QString storedBattery = readProfile(
        config->group(QString::fromLatin1(BatteryProfile))
            .group(QString::fromLatin1(PerformanceGroup)));
    const QString storedLowBattery = readProfile(
        config->group(QString::fromLatin1(LowBatteryProfile))
            .group(QString::fromLatin1(PerformanceGroup)));
    if (storedAc != m_acProfileId || storedBattery != m_batteryProfileId
        || storedLowBattery != m_lowBatteryProfileId) {
        m_acProfileId = storedAc;
        m_batteryProfileId = storedBattery;
        m_lowBatteryProfileId = storedLowBattery;
        Q_EMIT preferencesChanged();
    }
}

void PowerDevilProfileAdapter::ownerChanged(const QString &serviceName,
                                            const QString &oldOwner,
                                            const QString &newOwner)
{
    Q_UNUSED(serviceName);
    // AGENT-NOTE: mirrors PowerDevilLidAdapter/PowerDevilIdleAdapter.
    // Invalidate a reply from the previous owner before emitting any state
    // signal; consumers may apply synchronously from availabilityChanged and
    // that apply must own the next request serial.
    ++m_requestSerial;
    if (newOwner.isEmpty()) {
        setAvailable(false);
        if (m_applying) {
            finishApply(false, QStringLiteral("powerdevil-owner-lost"));
        }
        return;
    }

    const bool ownerReplaced = !oldOwner.isEmpty();
    setError({});
    if (ownerReplaced && m_applying) {
        finishApply(false, QStringLiteral("powerdevil-owner-replaced"));
    }

    setAvailable(true);
    if (!m_applying) {
        readPreferences();
        requestRefresh(true);
    }
}

void PowerDevilProfileAdapter::requestRefresh(const bool recovery)
{
    if (!m_available || !m_sessionBus.isConnected()) {
        if (!recovery) {
            finishApply(false, QStringLiteral("powerdevil-unavailable"));
        }
        return;
    }

    const quint64 serial = ++m_requestSerial;
    QDBusMessage call = QDBusMessage::createMethodCall(
        QString::fromLatin1(PowerDevilService), QString::fromLatin1(PowerDevilPath),
        QString::fromLatin1(PowerDevilInterface), QStringLiteral("refreshStatus"));
    auto *watcher = new QDBusPendingCallWatcher(m_sessionBus.asyncCall(call), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, serial, recovery] {
                const QDBusPendingReply<> reply = *watcher;
                watcher->deleteLater();
                if (serial != m_requestSerial) {
                    return;
                }
                if (reply.isError()) {
                    const QString error = dbusError(reply.error());
                    if (recovery) {
                        setError(error);
                    } else {
                        finishApply(false, error);
                    }
                    return;
                }
                if (recovery) {
                    setError({});
                } else {
                    finishApply(true, {});
                }
            });
}

void PowerDevilProfileAdapter::finishApply(const bool success,
                                           const QString &error)
{
    if (m_applying) {
        m_applying = false;
        Q_EMIT applyingChanged();
    }
    setError(error);
    Q_EMIT applyFinished(success, m_error);
}

void PowerDevilProfileAdapter::setAvailable(const bool available)
{
    if (m_available == available) {
        return;
    }
    m_available = available;
    Q_EMIT availabilityChanged();
}

void PowerDevilProfileAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    Q_EMIT errorChanged();
}

bool PowerDevilProfileAdapter::writePreferences(
    const QString &acProfileId, const QString &batteryProfileId,
    const QString &lowBatteryProfileId, QString *error) const
{
    const KSharedConfig::Ptr config =
        KSharedConfig::openConfig(QString::fromLatin1(PowerDevilConfig));
    if (!config || config->accessMode() != KConfigBase::ReadWrite) {
        if (error) {
            *error = QStringLiteral("powerdevil-config-not-writable");
        }
        return false;
    }

    config->reparseConfiguration();
    const auto writeOne = [&config](const char *profile, const QString &id) {
        KConfigGroup performance = config->group(QString::fromLatin1(profile))
                                       .group(QString::fromLatin1(PerformanceGroup));
        if (id.trimmed().isEmpty()) {
            performance.deleteEntry(QString::fromLatin1(PowerProfileKey));
        } else {
            performance.writeEntry(QString::fromLatin1(PowerProfileKey), id);
        }
    };
    writeOne(AcProfile, acProfileId);
    writeOne(BatteryProfile, batteryProfileId);
    writeOne(LowBatteryProfile, lowBatteryProfileId);

    if (!config->sync()) {
        if (error) {
            *error = QStringLiteral("powerdevil-config-sync-failed");
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Session::PowerDevilProfile
