// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/apps/settings_keyring/keyring_settings_model.h>
#include <QStringDecoder>
namespace QindaQt::Apps::SettingsKeyring {
using Services::KeyringClient::Request;
KeyringSettingsModel::KeyringSettingsModel(Services::KeyringClient::KeyringGateway &gateway,KeyringPreferences *preferences,QObject *parent):QObject(parent),m_gateway(gateway),m_preferences(preferences) {
    m_revealTimeout.setSingleShot(true);m_revealTimeout.setInterval(15000);
    connect(&m_revealTimeout,&QTimer::timeout,this,&KeyringSettingsModel::clearSecret);
    connect(&gateway,&Services::KeyringClient::KeyringGateway::authorityChanged,this,[this]{
        m_token=0;m_secretsAllowed=false;clearSecret();m_collections.clear();m_items.clear();m_selected.clear();
        m_status=available()?"":"Keyring unavailable";m_policyStatus="Policy observation unavailable";emit changed();
        if(m_active && available()) reload();
    });
    connect(&gateway,&Services::KeyringClient::KeyringGateway::secretsInvalidated,this,&KeyringSettingsModel::invalidateSecrets);
    connect(&gateway,&Services::KeyringClient::KeyringGateway::metadataChanged,this,[this]{if(m_active && !busy()) reload();});
    connect(&gateway,&Services::KeyringClient::KeyringGateway::policyChanged,this,[this](const QVariantMap &state){
        m_secretsAllowed=state.value("ScreenLockAvailable").toBool() && !state.value("ScreenLocked").toBool();
        if(!m_secretsAllowed) invalidateSecrets();
        if(!state.value("ScreenLockAvailable").toBool()) m_policyStatus="Screen lock state unavailable. Reveal and copy are disabled.";
        else if(state.value("ScreenLocked").toBool()) m_policyStatus="Unlock the screen to reveal or copy secrets.";
        else if(state.value("LockAfterIdleMinutes").toInt()>0 && !state.value("IdleAvailable").toBool()) m_policyStatus="Idle observation unavailable; collections stay locked";
        else if(!state.value("SettingsAvailable").toBool()) m_policyStatus="Settings1 unavailable; last confirmed policy retained";
        else m_policyStatus.clear();
        emit changed();
    });
    connect(&gateway,&Services::KeyringClient::KeyringGateway::rowsReady,this,&KeyringSettingsModel::rows);
    connect(&gateway,&Services::KeyringClient::KeyringGateway::actionFinished,this,[this](quint64 token,bool confirmed,const QString &message){
        if(token!=m_token || !token) return;
        m_token=0;m_status=message;emit changed();
        if(confirmed && m_active) reload();
    });
    connect(&gateway,&Services::KeyringClient::KeyringGateway::secretReady,this,[this](quint64 token,std::shared_ptr<qindaqt::keyring::SecureBuffer> bytes,const QString &){
        if(token!=m_token || !token || !m_active || !m_secretsAllowed) {if(bytes) bytes->clear();return;}
        m_token=0;
        if(m_copy) {m_status="Copy unavailable";emit copyRequested(bytes);m_copy=false;}
        else {m_secret=std::move(bytes);m_revealTimeout.start();m_status="Hidden automatically after 15 seconds";}
        emit changed();
    });
}
KeyringSettingsModel::~KeyringSettingsModel(){deactivate();}
bool KeyringSettingsModel::available() const {return m_gateway.available();}
QString KeyringSettingsModel::secretText() const {
    if(!m_secret) return {};
    if(m_secret->size()>65536) return QStringLiteral("Large secret — use Copy");
    const auto bytes=m_secret->bytes();QStringDecoder decode(QStringDecoder::Utf8);
    QString result=decode(QByteArrayView(reinterpret_cast<const char *>(bytes.data()),static_cast<qsizetype>(bytes.size())));
    return decode.hasError()?QStringLiteral("Binary secret — use Copy"):result;
}
void KeyringSettingsModel::invalidateSecrets(){
    m_secretsAllowed=false;m_copy=false;clearSecret();
    if(m_token && m_request==Request::Reveal) {m_gateway.cancel();m_token=0;m_status="Keyring locked; authenticate again";emit changed();}
}
void KeyringSettingsModel::clearSecret(){
    m_revealTimeout.stop();if(m_secret) m_secret->clear();m_secret.reset();emit changed();
}
void KeyringSettingsModel::acknowledgeCopy(bool confirmed){
    m_status=confirmed?"Copied for 30 seconds":"Copy unavailable";emit changed();
}
void KeyringSettingsModel::deactivate(){
    m_active=false;m_secretsAllowed=false;m_gateway.cancel();m_token=0;m_copy=false;clearSecret();emit deactivated();
}
void KeyringSettingsModel::begin(Request request,const QString &path,const QString &label){
    if(!m_active || busy() || !available() || !m_nextToken) return;
    clearSecret();m_request=request;m_token=m_nextToken++;m_status.clear();emit changed();
    m_gateway.request(m_token,request,path,label);
}
void KeyringSettingsModel::reload(){m_active=true;begin(Request::Collections);}
void KeyringSettingsModel::rows(quint64 token,const QVariantList &rows){
    if(!token || token!=m_token || !m_active) return;
    m_token=0;
    if(m_request==Request::Items) {m_items=rows;emit changed();return;}
    m_collections=rows;
    bool found=false;for(const auto &row:rows) if(row.toMap().value("path").toString()==m_selected) found=true;
    if(!found) m_selected=rows.isEmpty()?QString():rows.first().toMap().value("path").toString();
    m_items.clear();emit changed();
    if(!m_selected.isEmpty()) begin(Request::Items,m_selected);
}
void KeyringSettingsModel::selectCollection(const QString &path){
    if(busy()) return;
    for(const auto &row:m_collections) if(row.toMap().value("path").toString()==path) {
        m_selected=path;m_items.clear();begin(Request::Items,path);return;
    }
}
bool KeyringSettingsModel::knownItem(const QString &path) const {
    for(const auto &row:m_items) if(row.toMap().value("path").toString()==path
        && !row.toMap().value("locked").toBool() && row.toMap().value("indexAuthenticated").toBool()) return true;
    return false;
}
void KeyringSettingsModel::lockCollection(){if(!m_selected.isEmpty()) begin(Request::Lock,m_selected);}
void KeyringSettingsModel::unlockCollection(){if(!m_selected.isEmpty()) begin(Request::Unlock,m_selected);}
void KeyringSettingsModel::changePassword(){if(!m_selected.isEmpty()) begin(Request::ChangePassword,m_selected);}
void KeyringSettingsModel::createCollection(const QString &label){begin(Request::Create,{},label);}
void KeyringSettingsModel::revealItem(const QString &path){if(!busy() && available() && m_secretsAllowed && knownItem(path)) {m_copy=false;begin(Request::Reveal,path);}}
void KeyringSettingsModel::copyItem(const QString &path){if(!busy() && available() && m_secretsAllowed && knownItem(path)) {m_copy=true;begin(Request::Reveal,path);}}
void KeyringSettingsModel::deleteItem(const QString &path){if(knownItem(path)) begin(Request::Delete,path);}
}
