// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// AGENT-CONTRACT: stands in for phone/app/VaultBridge in the desktop preview. It must expose every
// property and invokable the vault QML uses with the same names and types (VaultBridge.h is the
// other side). All data is synthetic; the preview never touches a real vault or Keystore.
class FakeVault : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state MEMBER m_state NOTIFY changed)
    Q_PROPERTY(QString message MEMBER m_message NOTIFY changed)
    Q_PROPERTY(bool strongBox MEMBER m_strongBox NOTIFY changed)
    Q_PROPERTY(bool copyEnabled MEMBER m_copy NOTIFY changed)
    Q_PROPERTY(int restoreCount MEMBER m_restoreCount NOTIFY changed)
    Q_PROPERTY(QVariantList entries MEMBER m_entries NOTIFY entriesChanged)
    Q_PROPERTY(QString backupOp MEMBER m_backupOp NOTIFY changed)
    Q_PROPERTY(QString backupStatus MEMBER m_backupStatus NOTIFY changed)
    Q_PROPERTY(int backupCount MEMBER m_backupCount NOTIFY changed)
    Q_PROPERTY(bool active MEMBER m_active NOTIFY activeChanged)

public:
    static QStringList scenarios();
    void setScenario(const QString &name);
    bool secure() const { return m_secure; }

    Q_INVOKABLE void unlock();
    Q_INVOKABLE void lock() { setScenario(QStringLiteral("locked")); }
    Q_INVOKABLE void touch() {}
    Q_INVOKABLE void setSecure(bool on) { m_secure = on; }
    Q_INVOKABLE void eraseVault() { setScenario(QStringLiteral("none")); }
    Q_INVOKABLE void setCopyEnabled(bool on) { m_copy = on; emit changed(); }
    Q_INVOKABLE QVariantMap entry(const QString &id);
    Q_INVOKABLE QVariantMap reveal(const QString &id);
    Q_INVOKABLE QString save(const QVariantMap &entry);
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool copyPassword(const QString &) { return m_copy; }
    Q_INVOKABLE QString generatePassword(int length, bool lower, bool upper, bool digits, bool symbols, bool avoidAmbiguous);
    Q_INVOKABLE QString generatePassphrase(int words, const QString &separator, bool capitalize, bool number);
    Q_INVOKABLE QVariantMap passphraseHint(const QString &passphrase);
    Q_INVOKABLE QString exportBackup(const QString &pass, const QString &confirm);
    Q_INVOKABLE QString importBackup(const QString &pass, const QString &mode);
    Q_INVOKABLE void clearBackupStatus() { m_backupOp.clear(); m_backupStatus = QStringLiteral("none"); emit changed(); }

signals:
    void changed();
    void entriesChanged();
    void activeChanged();

private:
    QString m_state = QStringLiteral("locked"), m_message, m_backupOp, m_backupStatus = QStringLiteral("none");
    bool m_strongBox = true, m_copy = false, m_active = false, m_secure = false;
    int m_restoreCount = 0, m_backupCount = 0;
    QVariantList m_entries;
    QVariantMap m_secrets;   // id -> {password, notes}
};
