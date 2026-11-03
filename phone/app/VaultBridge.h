// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// AGENT-CONTRACT: the Qt side of the password vault; the Java side is
// org.qindaqt.link.android.VaultBridge (static methods looked up by name, JSON in and out). Both must
// change together, and phone/preview/FakeVault.h must expose the same properties and invokables.
// Secrets cross only through reveal(), save() and copyPassword(); nothing here logs a value.
// Ownership: lives on the GUI thread. While `active` is false (the vault page is not shown) nothing is
// polled; the Java side keeps its own timers and locks regardless of the UI.
class VaultBridge : public QObject
{
    Q_OBJECT
    // state: none | locked | authenticating | unlocked | invalidated | nolock | error | restore_ready
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool strongBox READ strongBox NOTIFY changed)
    Q_PROPERTY(bool copyEnabled READ copyEnabled NOTIFY changed)
    Q_PROPERTY(int restoreCount READ restoreCount NOTIFY changed)
    // Non-secret list fields: [{id, title, username, url, updated}].
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    // op: export | import | ""; status: none | working | picking | done | cancelled | failed | wrong-passphrase.
    Q_PROPERTY(QString backupOp READ backupOp NOTIFY changed)
    Q_PROPERTY(QString backupStatus READ backupStatus NOTIFY changed)
    Q_PROPERTY(int backupCount READ backupCount NOTIFY changed)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    explicit VaultBridge(QObject *parent = nullptr);

    QString state() const { return m_s.value(QStringLiteral("state")).toString(); }
    QString message() const { return m_s.value(QStringLiteral("message")).toString(); }
    bool strongBox() const { return m_s.value(QStringLiteral("strongBox")).toBool(); }
    bool copyEnabled() const { return m_s.value(QStringLiteral("copyEnabled")).toBool(); }
    int restoreCount() const { return m_s.value(QStringLiteral("restoreCount")).toInt(); }
    QVariantList entries() const { return m_entries; }
    QString backupOp() const { return m_s.value(QStringLiteral("backup")).toMap().value(QStringLiteral("op")).toString(); }
    QString backupStatus() const { return m_s.value(QStringLiteral("backup")).toMap().value(QStringLiteral("status")).toString(); }
    int backupCount() const { return m_s.value(QStringLiteral("backup")).toMap().value(QStringLiteral("count")).toInt(); }
    bool active() const { return m_active; }
    void setActive(bool on);

    Q_INVOKABLE void unlock();            // unlocks, or creates the vault when none exists
    Q_INVOKABLE void lock();
    Q_INVOKABLE void touch();
    Q_INVOKABLE void setSecure(bool on);  // FLAG_SECURE on the activity window
    Q_INVOKABLE void eraseVault();
    Q_INVOKABLE void setCopyEnabled(bool on);
    Q_INVOKABLE QVariantMap entry(const QString &id);     // everything but password and notes
    Q_INVOKABLE QVariantMap reveal(const QString &id);    // {password, notes}; the caller drops it on hide
    Q_INVOKABLE QString save(const QVariantMap &entry);   // "ok:<id>" or "err:<reason>"
    Q_INVOKABLE bool remove(const QString &id);
    Q_INVOKABLE bool copyPassword(const QString &id);
    Q_INVOKABLE QString generatePassword(int length, bool lower, bool upper, bool digits, bool symbols, bool avoidAmbiguous);
    Q_INVOKABLE QString generatePassphrase(int words, const QString &separator, bool capitalize, bool number);
    Q_INVOKABLE QVariantMap passphraseHint(const QString &passphrase);
    Q_INVOKABLE QString exportBackup(const QString &pass, const QString &confirm);   // "" or a reason
    Q_INVOKABLE QString importBackup(const QString &pass, const QString &mode);      // merge | replace | restore
    Q_INVOKABLE void clearBackupStatus();
    // "Type on desktop" (ADR-0440). field: username | password | autotype. typeState() is
    // {phase, app, title, message}: idle | requesting | confirm | need-unlock | sending | done | failed.
    Q_INVOKABLE void typeBegin(const QString &id, const QString &field);
    Q_INVOKABLE void typeConfirm();
    Q_INVOKABLE void typeCancel();
    Q_INVOKABLE QVariantMap typeState();

signals:
    void changed();
    void entriesChanged();
    void activeChanged();

private:
    void poll();
    QTimer m_timer;
    QVariantMap m_s;
    QVariantList m_entries;
    int m_rev = -1;
    bool m_active = false;
};
