// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_keyring/keyring_preferences.h>
namespace QindaQt::Apps::SettingsKeyring {
namespace sc=Services::SettingsClient;
namespace sp=Services::SettingsProtocol;
QStringList KeyringPreferences::scopedKeys(){return {"keyring.lockOnScreenLock","keyring.lockAfterIdleMinutes"};}
KeyringPreferences::KeyringPreferences(sc::SettingsClient &client,QObject *parent):QObject(parent),m_client(client){
    connect(&client,&sc::SettingsClient::snapshotChanged,this,&KeyringPreferences::snapshot);
    connect(&client,&sc::SettingsClient::stateChanged,this,[this]{snapshot();emit changed();});
    connect(&client,&sc::SettingsClient::ownerChanged,this,[this]{emit changed();});
    connect(&client,&sc::SettingsClient::writeInFlightChanged,this,[this]{emit changed();});
    connect(&client,&sc::SettingsClient::commitUncertain,this,[this](const QString &){m_status="Save not confirmed; refresh before trying again";emit changed();});
    connect(&client,&sc::SettingsClient::commitFinished,this,[this](const sc::CommitOutcome &outcome){
        m_status=outcome.status==sp::SettingsWireStatus::Applied?"Preferences saved":"Preferences were not saved";snapshot();emit changed();
    });
    snapshot();
}
bool KeyringPreferences::available() const {
    return m_client.state()==sc::ClientState::Ready && m_client.snapshot()
        && m_client.snapshot()->owner==m_client.currentOwner();
}
bool KeyringPreferences::busy() const {return m_client.writeInFlight();}
void KeyringPreferences::snapshot(){
    if(!available()) return;
    const auto &values=m_client.snapshot()->values;
    const auto lock=values.value("keyring.lockOnScreenLock"),idle=values.value("keyring.lockAfterIdleMinutes");
    const auto type=idle.metaType().id();
    const bool numeric=type==QMetaType::Int || type==QMetaType::UInt || type==QMetaType::LongLong || type==QMetaType::ULongLong || type==QMetaType::Double;
    if(lock.metaType()!=QMetaType::fromType<bool>() || !numeric) return;
    bool ok=false;const auto minutes=idle.toInt(&ok);
    if(!ok || minutes<0 || minutes>1440 || idle.toDouble()!=minutes) return;
    m_lockOnScreenLock=lock.toBool();m_idleMinutes=minutes;emit changed();
}
void KeyringPreferences::write(const QString &key,const QVariant &value){
    QString error;
    if(!available() || !m_client.setUserValue(key,value,&error)) m_status="Preferences unavailable or busy";
    else m_status="Saving preferences…";
    emit changed();
}
void KeyringPreferences::setLockOnScreenLock(bool value){write("keyring.lockOnScreenLock",value);}
void KeyringPreferences::setLockAfterIdleMinutes(int value){
    if(value<0 || value>1440) {m_status="Idle timeout must be 0–1440 minutes";emit changed();return;}
    write("keyring.lockAfterIdleMinutes",value);
}
}
