// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/settings_client/settings_client.h>
#include <QObject>
namespace QindaQt::Apps::SettingsKeyring {
// Same-thread adapter; borrows an already scoped SettingsClient that outlives it.
// Only confirmed matching-owner snapshots change effective values. Last confirmed
// policy is retained while unavailable; writes never optimistically claim save.
class KeyringPreferences final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool lockOnScreenLock READ lockOnScreenLock NOTIFY changed)
    Q_PROPERTY(int lockAfterIdleMinutes READ lockAfterIdleMinutes NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit KeyringPreferences(Services::SettingsClient::SettingsClient &,QObject *parent=nullptr);
    bool available() const;
    bool busy() const;
    bool lockOnScreenLock() const {return m_lockOnScreenLock;}
    int lockAfterIdleMinutes() const {return m_idleMinutes;}
    QString status() const {return m_status;}
    Q_INVOKABLE void setLockOnScreenLock(bool);
    Q_INVOKABLE void setLockAfterIdleMinutes(int);
    static QStringList scopedKeys();
Q_SIGNALS:
    void changed();
private:
    void snapshot();
    void write(const QString &,const QVariant &);
    Services::SettingsClient::SettingsClient &m_client;
    QString m_status;
    bool m_lockOnScreenLock=false;
    int m_idleMinutes=0;
};
}
