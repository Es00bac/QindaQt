// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/session/powerdevil_idle/powerdevil_idle_adapter.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

#include <array>
#include <utility>

namespace QindaQt::Session::PowerDevilIdle {
namespace {

constexpr auto PowerDevilService = "org.kde.Solid.PowerManagement";
constexpr auto PowerDevilPath = "/org/kde/Solid/PowerManagement";
constexpr auto PowerDevilInterface = "org.kde.Solid.PowerManagement";
constexpr auto PowerDevilConfig = "powerdevilrc";
constexpr auto DisplayGroup = "Display";
constexpr auto TurnOffEnabledKey = "TurnOffDisplayWhenIdle";
constexpr auto TurnOffTimeoutKey = "TurnOffDisplayIdleTimeoutSec";
constexpr std::array Profiles{"AC", "Battery", "LowBattery"};

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

} // namespace

PowerDevilIdleAdapter::PowerDevilIdleAdapter(QDBusConnection sessionBus,
                                             QObject *parent)
    : QObject(parent)
    , m_sessionBus(std::move(sessionBus))
{
}

PowerDevilIdleAdapter::~PowerDevilIdleAdapter()
{
    stop();
}

void PowerDevilIdleAdapter::start()
{
    if (m_serviceWatcher != nullptr) {
        return;
    }
    if (m_sessionBus.isConnected()) {
        m_serviceWatcher = new QDBusServiceWatcher(
            QString::fromLatin1(PowerDevilService), m_sessionBus,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
                this, &PowerDevilIdleAdapter::ownerChanged);
    }
    refreshAvailability();
}

void PowerDevilIdleAdapter::stop()
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

bool PowerDevilIdleAdapter::available() const noexcept
{
    return m_available;
}

bool PowerDevilIdleAdapter::enabled() const noexcept
{
    return m_enabled;
}

int PowerDevilIdleAdapter::minutes() const noexcept
{
    return m_minutes;
}

bool PowerDevilIdleAdapter::applying() const noexcept
{
    return m_applying;
}

const QString &PowerDevilIdleAdapter::error() const noexcept
{
    return m_error;
}

bool PowerDevilIdleAdapter::apply(const bool enabled, const int minutes)
{
    if (m_applying) {
        setError(QStringLiteral("powerdevil-apply-busy"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }
    if (minutes < MinimumMinutes || minutes > MaximumMinutes) {
        setError(QStringLiteral("idle-timeout-out-of-range"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }
    if (!m_available) {
        setError(QStringLiteral("powerdevil-unavailable"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }

    QString writeError;
    if (!writePreferences(enabled, minutes, &writeError)) {
        setError(writeError);
        Q_EMIT applyFinished(false, m_error);
        return false;
    }

    m_enabled = enabled;
    m_minutes = minutes;
    Q_EMIT preferencesChanged();
    setError({});
    m_applying = true;
    Q_EMIT applyingChanged();
    requestRefresh(false);
    return true;
}

void PowerDevilIdleAdapter::refreshAvailability()
{
    setAvailable(!ownerFor(m_sessionBus).isEmpty());
}

void PowerDevilIdleAdapter::ownerChanged(const QString &serviceName,
                                         const QString &oldOwner,
                                         const QString &newOwner)
{
    Q_UNUSED(serviceName);
    // Invalidate a reply from the previous owner before emitting any state
    // signal. Consumers may synchronously apply their current preference from
    // availabilityChanged, and that apply must own the next request serial.
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

    // A newly registered or replacement daemon must reread the file. If the
    // availability signal synchronously started an apply, that apply already
    // requested the refresh; issuing a recovery request here would supersede
    // it and strand the consumer's applying state.
    setAvailable(true);
    if (!m_applying) {
        requestRefresh(true);
    }
}

void PowerDevilIdleAdapter::requestRefresh(const bool recovery)
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

void PowerDevilIdleAdapter::finishApply(const bool success,
                                        const QString &error)
{
    if (m_applying) {
        m_applying = false;
        Q_EMIT applyingChanged();
    }
    setError(error);
    Q_EMIT applyFinished(success, m_error);
}

void PowerDevilIdleAdapter::setAvailable(const bool available)
{
    if (m_available == available) {
        return;
    }
    m_available = available;
    Q_EMIT availabilityChanged();
}

void PowerDevilIdleAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    Q_EMIT errorChanged();
}

bool PowerDevilIdleAdapter::writePreferences(const bool enabled,
                                             const int minutes,
                                             QString *error) const
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
    const int seconds = minutes * 60;
    for (const auto profile : Profiles) {
        KConfigGroup display = config->group(QString::fromLatin1(profile))
                                   .group(QString::fromLatin1(DisplayGroup));
        display.writeEntry(QString::fromLatin1(TurnOffEnabledKey), enabled);
        display.writeEntry(QString::fromLatin1(TurnOffTimeoutKey), seconds);
    }
    if (!config->sync()) {
        if (error) {
            *error = QStringLiteral("powerdevil-config-sync-failed");
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Session::PowerDevilIdle
