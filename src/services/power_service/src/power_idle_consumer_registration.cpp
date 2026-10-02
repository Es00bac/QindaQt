// SPDX-License-Identifier: GPL-3.0-or-later
#include "power_service_object_p.h"
#include "idle_consumer_authority_p.h"
namespace QindaQt::Power {
namespace {
constexpr quint32 CompleteIdleScopes = static_cast<quint32>(IdleInhibitorScope::AutomaticLock)
    | static_cast<quint32>(IdleInhibitorScope::DisplayOff)
    | static_cast<quint32>(IdleInhibitorScope::IdleSuspend);
}
void PowerServiceObject::setNativeIdleAdmission(const bool admitted) {
    m_idleConsumerAuthority->setNativeAdmission(admitted);
}
void PowerServiceObject::clearIdleConsumers() {
    for (const QString &owner : m_watchedOwners) m_ownerWatcher->removeWatchedService(owner);
    m_watchedOwners.clear();
    m_idleInhibitors.setConsumedScopes({});
    Q_EMIT IdleInhibitorsChanged(0, 0);
}
bool PowerServiceObject::RegisterIdleConsumers(const quint64 expectedEpoch,
                                               const quint32 scopes) {
    if (!calledFromDBus()) return false;
    synchronizeIdleInhibitorEpoch(m_coordinator->snapshot().epoch);
    // AGENT-GUARD: advertise no partial shared-policy composition. ScreenSaver
    // inhibits all three stages atomically; only the current actual Session1
    // owner may admit complete consumers or withdraw them in this epoch.
    if (!m_idleConsumerAuthority->accepts(message().service()) || expectedEpoch == 0
        || expectedEpoch != m_idleInhibitorEpoch
        || (scopes != 0 && scopes != CompleteIdleScopes)) return false;
    if (scopes == 0) { clearIdleConsumers(); return true; }
    m_idleInhibitors.setConsumedScopes(IdleInhibitorScopes::fromInt(scopes));
    Q_EMIT IdleInhibitorsChanged(scopes, activeIdleInhibitorScopes());
    return true;
}
}
