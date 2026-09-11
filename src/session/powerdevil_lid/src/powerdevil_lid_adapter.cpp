// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/session/powerdevil_lid/powerdevil_lid_adapter.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QtDBus/QDBusConnectionInterface>
#include <QtDBus/QDBusPendingCallWatcher>
#include <QtDBus/QDBusPendingReply>
#include <QtDBus/QDBusReply>
#include <QtDBus/QDBusServiceWatcher>

#include <array>
#include <utility>

namespace QindaQt::Session::PowerDevilLid {
namespace {

constexpr auto PowerDevilService = "org.kde.Solid.PowerManagement";
constexpr auto PowerDevilPath = "/org/kde/Solid/PowerManagement";
constexpr auto PowerDevilInterface = "org.kde.Solid.PowerManagement";
constexpr auto PowerDevilConfig = "powerdevilrc";
constexpr auto SuspendAndShutdownGroup = "SuspendAndShutdown";
constexpr auto LidActionKey = "LidAction";
constexpr auto InhibitLidKey = "InhibitLidActionWhenExternalMonitorPresent";
constexpr auto PowerButtonActionKey = "PowerButtonAction";
constexpr std::array Profiles{"AC", "Battery", "LowBattery"};

// AGENT-GUARD: keep this list in step with the public action constants in the
// header; it is the complete set the adapter may ever write. A value outside
// the cited upstream v6.6.6 enum is a foreign action and must be rejected.
constexpr std::array SupportedActions{
    PowerDevilLidAdapter::DoNothing, PowerDevilLidAdapter::Sleep,
    PowerDevilLidAdapter::Hibernate, PowerDevilLidAdapter::ShutDown,
    PowerDevilLidAdapter::LockScreen, PowerDevilLidAdapter::TurnOffScreen};

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

PowerDevilLidAdapter::PowerDevilLidAdapter(QDBusConnection sessionBus,
                                           QObject *parent)
    : QObject(parent)
    , m_sessionBus(std::move(sessionBus))
{
}

PowerDevilLidAdapter::~PowerDevilLidAdapter()
{
    stop();
}

bool PowerDevilLidAdapter::isSupportedAction(const quint32 action)
{
    return std::find(SupportedActions.begin(), SupportedActions.end(), action)
        != SupportedActions.end();
}

void PowerDevilLidAdapter::start()
{
    if (m_serviceWatcher != nullptr) {
        return;
    }
    if (m_sessionBus.isConnected()) {
        m_serviceWatcher = new QDBusServiceWatcher(
            QString::fromLatin1(PowerDevilService), m_sessionBus,
            QDBusServiceWatcher::WatchForOwnerChange, this);
        connect(m_serviceWatcher, &QDBusServiceWatcher::serviceOwnerChanged,
                this, &PowerDevilLidAdapter::ownerChanged);
    }
    refreshAvailability();
}

void PowerDevilLidAdapter::stop()
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

bool PowerDevilLidAdapter::available() const noexcept
{
    return m_available;
}

bool PowerDevilLidAdapter::applying() const noexcept
{
    return m_applying;
}

const QString &PowerDevilLidAdapter::error() const noexcept
{
    return m_error;
}

bool PowerDevilLidAdapter::apply(const quint32 lidAction,
                                 const bool inhibitLidActionWhenExternalMonitorPresent,
                                 const quint32 powerButtonAction)
{
    if (m_applying) {
        setError(QStringLiteral("powerdevil-apply-busy"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }
    if (!isSupportedAction(lidAction)) {
        setError(QStringLiteral("lid-action-unsupported"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }
    if (!isSupportedAction(powerButtonAction)) {
        setError(QStringLiteral("power-button-action-unsupported"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }
    if (!m_available) {
        setError(QStringLiteral("powerdevil-unavailable"));
        Q_EMIT applyFinished(false, m_error);
        return false;
    }

    QString writeError;
    if (!writePreferences(lidAction, inhibitLidActionWhenExternalMonitorPresent,
                          powerButtonAction, &writeError)) {
        setError(writeError);
        Q_EMIT applyFinished(false, m_error);
        return false;
    }

    setError({});
    m_applying = true;
    Q_EMIT applyingChanged();
    requestRefresh(false);
    return true;
}

void PowerDevilLidAdapter::refreshAvailability()
{
    setAvailable(!ownerFor(m_sessionBus).isEmpty());
}

void PowerDevilLidAdapter::ownerChanged(const QString &serviceName,
                                        const QString &oldOwner,
                                        const QString &newOwner)
{
    Q_UNUSED(serviceName);
    // AGENT-NOTE: mirrors PowerDevilIdleAdapter. Invalidate a reply from the
    // previous owner before emitting any state signal; consumers may apply
    // synchronously from availabilityChanged and that apply must own the next
    // request serial.
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
        requestRefresh(true);
    }
}

void PowerDevilLidAdapter::requestRefresh(const bool recovery)
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

void PowerDevilLidAdapter::finishApply(const bool success,
                                       const QString &error)
{
    if (m_applying) {
        m_applying = false;
        Q_EMIT applyingChanged();
    }
    setError(error);
    Q_EMIT applyFinished(success, m_error);
}

void PowerDevilLidAdapter::setAvailable(const bool available)
{
    if (m_available == available) {
        return;
    }
    m_available = available;
    Q_EMIT availabilityChanged();
}

void PowerDevilLidAdapter::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    Q_EMIT errorChanged();
}

bool PowerDevilLidAdapter::writePreferences(
    const quint32 lidAction,
    const bool inhibitLidActionWhenExternalMonitorPresent,
    const quint32 powerButtonAction,
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
    for (const auto profile : Profiles) {
        KConfigGroup suspendAndShutdown =
            config->group(QString::fromLatin1(profile))
                .group(QString::fromLatin1(SuspendAndShutdownGroup));
        suspendAndShutdown.writeEntry(QString::fromLatin1(LidActionKey), lidAction);
        suspendAndShutdown.writeEntry(QString::fromLatin1(InhibitLidKey),
                                      inhibitLidActionWhenExternalMonitorPresent);
        suspendAndShutdown.writeEntry(QString::fromLatin1(PowerButtonActionKey),
                                      powerButtonAction);
    }
    if (!config->sync()) {
        if (error) {
            *error = QStringLiteral("powerdevil-config-sync-failed");
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Session::PowerDevilLid
