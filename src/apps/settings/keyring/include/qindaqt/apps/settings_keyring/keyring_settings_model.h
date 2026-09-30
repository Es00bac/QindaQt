// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring_client/keyring_gateway.h>
#include <QObject>
#include "keyring_preferences.h"
#include <QTimer>
namespace QindaQt::Apps::SettingsKeyring {
// GUI-thread presentation state; borrows a gateway that must outlive it.
// Passwords never cross this model. Secret text is a short-lived Qt/UI copy,
// not a universal locked-memory guarantee. Page departure must call deactivate().
class KeyringSettingsModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *preferences READ preferences CONSTANT)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString policyStatus READ policyStatus NOTIFY changed)
    Q_PROPERTY(QVariantList collections READ collections NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QString selectedCollectionPath READ selectedCollectionPath NOTIFY changed)
    Q_PROPERTY(bool secretsAllowed READ secretsAllowed NOTIFY changed)
    Q_PROPERTY(bool secretVisible READ secretVisible NOTIFY changed)
    Q_PROPERTY(QString secretText READ secretText NOTIFY changed)
public:
    explicit KeyringSettingsModel(Services::KeyringClient::KeyringGateway &,KeyringPreferences *preferences=nullptr,QObject *parent=nullptr);
    ~KeyringSettingsModel() override;
    QObject *preferences() const {return m_preferences;}
    bool available() const;
    bool busy() const {return m_token!=0;}
    QString status() const {return m_status;}
    QString policyStatus() const {return m_policyStatus;}
    QVariantList collections() const {return m_collections;}
    QVariantList items() const {return m_items;}
    QString selectedCollectionPath() const {return m_selected;}
    bool secretsAllowed() const {return m_secretsAllowed;}
    bool secretVisible() const {return static_cast<bool>(m_secret);}
    QString secretText() const;
    Q_INVOKABLE void reload();
    Q_INVOKABLE void selectCollection(const QString &path);
    Q_INVOKABLE void lockCollection();
    Q_INVOKABLE void unlockCollection();
    Q_INVOKABLE void changePassword();
    Q_INVOKABLE void createCollection(const QString &label);
    Q_INVOKABLE void revealItem(const QString &path);
    Q_INVOKABLE void copyItem(const QString &path);
    Q_INVOKABLE void deleteItem(const QString &path);
    Q_INVOKABLE void clearSecret();
    Q_INVOKABLE void acknowledgeCopy(bool confirmed);
    Q_INVOKABLE void deactivate();
Q_SIGNALS:
    void changed();
    // Page lifetime retirement is synchronous on the GUI thread. Composition
    // clears its separately owned clipboard before retaining an inactive route.
    void deactivated();
    // Composition owns the clipboard policy; bytes are borrowed synchronously
    // and must be copied into an owning sensitive MIME provider before return.
    void copyRequested(std::shared_ptr<qindaqt::keyring::SecureBuffer> bytes);
private:
    void invalidateSecrets();
    void begin(Services::KeyringClient::Request,const QString &path={},const QString &label={});
    bool knownItem(const QString &) const;
    void rows(quint64,const QVariantList &);
    Services::KeyringClient::KeyringGateway &m_gateway;
    KeyringPreferences *m_preferences; // Optional borrowed same-thread adapter outlives model.
    QVariantList m_collections,m_items;
    QString m_selected,m_status,m_policyStatus=QStringLiteral("Policy observation unavailable");
    std::shared_ptr<qindaqt::keyring::SecureBuffer> m_secret;
    QTimer m_revealTimeout;
    quint64 m_nextToken=1,m_token=0;
    Services::KeyringClient::Request m_request=Services::KeyringClient::Request::Collections;
    bool m_copy=false,m_active=true,m_secretsAllowed=false;
};
}
