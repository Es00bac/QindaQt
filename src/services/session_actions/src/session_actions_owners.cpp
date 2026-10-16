// SPDX-License-Identifier: LGPL-3.0-or-later
// Event-driven owner cache and refresh coalescing (ADR-0379).
#include <qindaqt/services/session_actions/session_actions_client.h>
#include "session_actions_internal.h"
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <unistd.h>
namespace QindaQt::Services::SessionActions {
using namespace Detail;
namespace {
constexpr int OwnerLookupTimeoutMilliseconds = 2'000;
QString systemKey(const QString &service) { return QStringLiteral("system:") + service; }
}

void SessionActionsClient::installWatcher(QDBusServiceWatcher *&slot, const char *service,
                                          const QDBusConnection &connection,
                                          SessionAction authority)
{
    const QString name = QString::fromLatin1(service);
    const bool system = authority == SessionAction::Reboot;
    const QString key = system ? systemKey(name) : name;
    slot = new QDBusServiceWatcher(name, connection,
                                   QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(slot, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this, key, authority](const QString &, const QString &, const QString &owner) {
                if (cachedOwner(key) == owner && m_ownerSerials.value(key) != 0) {
                    return; // queued advertisement of the cached owner
                }
                noteOwner(key, owner);
                advanceAuthorityEpoch(authority);
                ++m_refreshSerial;
                publishAvailability({});
                if (authority == SessionAction::Logout) {
                    verifyScreenPowerOwner();
                }
                scheduleRefresh();
            });
    resolveInitialOwner(connection, key, name);
}

void SessionActionsClient::noteOwner(const QString &key, const QString &owner)
{
    ++m_ownerSerials[key];
    m_owners.insert(key, owner);
}

void SessionActionsClient::resolveInitialOwner(const QDBusConnection &connection,
                                               const QString &key, const QString &service)
{
    const quint64 serial = m_ownerSerials.value(key);
    auto *watcher = new QDBusPendingCallWatcher(
        connection.asyncCall(daemonCall("GetNameOwner", service),
                             OwnerLookupTimeoutMilliseconds), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, watcher, key, serial] {
                const QDBusPendingReply<QString> reply = *watcher;
                watcher->deleteLater();
                // A watcher event after the lookup was sent is newer truth.
                if (!m_running || m_ownerSerials.value(key) != serial) {
                    return;
                }
                noteOwner(key, reply.isError() ? QString{} : reply.value());
                if (key == QLatin1String(ScreenPowerService)
                    || key == QLatin1String(SessionService)) {
                    verifyScreenPowerOwner();
                }
                scheduleRefresh();
            });
}

QString SessionActionsClient::cachedOwner(const QString &key) const
{
    return m_owners.value(key);
}

QString SessionActionsClient::currentOwner(SessionAction action) const
{
    if (action == SessionAction::Lock) {
        return cachedOwner(QString::fromLatin1(ScreenSaverService));
    }
    if (isSleepAction(action)) {
        const auto owner = cachedOwner(QString::fromLatin1(SleepService));
        return owner == cachedOwner(QString::fromLatin1(SessionService)) ? owner : QString{};
    }
    if (action == SessionAction::Logout) {
        return cachedOwner(QString::fromLatin1(SessionService));
    }
    return cachedOwner(systemKey(QString::fromLatin1(LogindService)));
}

void SessionActionsClient::verifyScreenPowerOwner()
{
    const auto owner = cachedOwner(QString::fromLatin1(ScreenPowerService));
    if (owner.isEmpty() || owner != cachedOwner(QString::fromLatin1(SessionService))) {
        m_screenPowerVerified.clear();
        ++m_screenPowerCheck;
        return;
    }
    if (owner == m_screenPowerVerified) {
        return;
    }
    m_screenPowerVerified.clear();
    const quint64 check = ++m_screenPowerCheck;
    // One asynchronous same-UID proof per new unique owner; a bus daemon never
    // reuses a unique name, so the verdict stays valid for that owner.
    auto *watcher = new QDBusPendingCallWatcher(
        m_sessionBus.asyncCall(daemonCall("GetConnectionUnixUser", owner),
                               OwnerLookupTimeoutMilliseconds), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, owner, check] {
        const QDBusPendingReply<uint> reply = *watcher;
        watcher->deleteLater();
        if (!m_running || check != m_screenPowerCheck || reply.isError()
            || reply.value() != static_cast<uint>(::getuid())) {
            return;
        }
        m_screenPowerVerified = owner;
        scheduleRefresh();
    });
}

bool SessionActionsClient::refreshInFlight() const noexcept
{
    return m_inFlightSerial != 0 && m_inFlightSerial == m_refreshSerial;
}

void SessionActionsClient::scheduleRefresh()
{
    if (!m_running) {
        return;
    }
    // AGENT-GUARD: one running refresh plus one follow-up. A Changed burst
    // must never multiply the Can* round trips it answers.
    if (refreshInFlight()) {
        m_refreshAgain = true;
        return;
    }
    if (!m_refreshDebounce.isActive()) {
        m_refreshDebounce.start();
    }
}

void SessionActionsClient::refreshFinished()
{
    m_inFlightSerial = 0;
    if (m_refreshAgain) {
        m_refreshAgain = false;
        scheduleRefresh();
    }
}

} // namespace QindaQt::Services::SessionActions
