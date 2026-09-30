// SPDX-License-Identifier: GPL-3.0-or-later
#include "lock_policy.h"
namespace qindaqt::keyring::service {
namespace sc=QindaQt::Services::SettingsClient;
namespace sl=QindaQt::Services::SessionLockState;
KeyringLockPolicy::KeyringLockPolicy(sc::SettingsClient &settings,LockObservation &screen,
    IdleObservation &idle,std::function<void()> lockAll,QObject *parent)
    :QObject(parent),settings_(settings),screen_(screen),idle_(idle),lockAll_(std::move(lockAll)) {
    connect(&settings,&sc::SettingsClient::snapshotChanged,this,&KeyringLockPolicy::snapshot);
    connect(&settings,&sc::SettingsClient::stateChanged,this,[this]{snapshot();emit changed();});
    connect(&settings,&sc::SettingsClient::ownerChanged,this,[this]{emit changed();});
    connect(&screen,&LockObservation::changed,this,[this]{enforce();emit changed();});
    connect(&idle,&IdleObservation::changed,this,[this]{enforce();emit changed();});
    snapshot();
}
void KeyringLockPolicy::snapshot(){
    if(settings_.state()!=sc::ClientState::Ready || !settings_.snapshot()
        || settings_.snapshot()->owner!=settings_.currentOwner()) return;
    const auto values=settings_.snapshot()->values;
    const auto lock=values.value("keyring.lockOnScreenLock"),idle=values.value("keyring.lockAfterIdleMinutes");
    const auto type=idle.metaType().id();
    const bool numeric=type==QMetaType::Int || type==QMetaType::UInt || type==QMetaType::LongLong || type==QMetaType::ULongLong || type==QMetaType::Double;
    bool ok=false;const auto minutes=idle.toInt(&ok);
    if(lock.metaType()!=QMetaType::fromType<bool>() || !numeric || !ok
        || minutes<0 || minutes>1440 || idle.toDouble()!=minutes) return;
    lockOnScreen_=lock.toBool();idleMinutes_=minutes;
    idle_.setTimeout(minutes*60000);enforce();emit changed();
}
void KeyringLockPolicy::enforce(){
    if(enforcing_) return;
    const bool screen=lockOnScreen_ && screen_.state()!=sl::LockState::Unlocked;
    const bool idle=idleMinutes_>0 && (!idle_.available() || idle_.idle());
    if(!screen && !idle) return;
    enforcing_=true;lockAll_();enforcing_=false;
}
QVariantMap KeyringLockPolicy::status() const {
    const bool settings=settings_.state()==sc::ClientState::Ready && settings_.snapshot()
        && settings_.snapshot()->owner==settings_.currentOwner();
    return {{"SettingsAvailable",settings},{"ScreenLockAvailable",screen_.state()!=sl::LockState::Unknown},
        {"IdleAvailable",idle_.available()},{"ScreenLocked",screen_.state()!=sl::LockState::Unlocked},{"LockOnScreenLock",lockOnScreen_},{"LockAfterIdleMinutes",idleMinutes_}};
}
}
