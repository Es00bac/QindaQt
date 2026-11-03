// SPDX-License-Identifier: LGPL-3.0-or-later
#include "FakeVault.h"

QStringList FakeVault::scenarios()
{
    return {QStringLiteral("none"), QStringLiteral("nolock"), QStringLiteral("locked"), QStringLiteral("authenticating"),
            QStringLiteral("unlocked"), QStringLiteral("empty"), QStringLiteral("invalidated"), QStringLiteral("error"),
            QStringLiteral("restore_ready")};
}

static QVariantMap row(const QString &id, const QString &title, const QString &user, const QString &url)
{
    return {{"id", id}, {"title", title}, {"username", user}, {"url", url}, {"updated", 1700000000000LL}};
}

void FakeVault::setScenario(const QString &name)
{
    m_message.clear(); m_entries.clear(); m_secrets.clear(); m_restoreCount = 0;
    m_backupOp.clear(); m_backupStatus = QStringLiteral("none"); m_backupCount = 0;
    m_state = name == QLatin1String("empty") ? QStringLiteral("unlocked") : name;
    if (name == QLatin1String("unlocked")) {
        m_entries = {row("a1", "Example Mail", "alex@example.invalid", "https://mail.example.invalid"),
                     row("a2", "Test Bank", "tester01", "https://bank.example.invalid/login"),
                     row("a3", "Sample Forum", "sample_user", "https://forum.example.invalid")};
        m_secrets["a1"] = QVariantMap{{"password", "Synthetic-Pass-1!"}, {"notes", "synthetic note"}};
        m_secrets["a2"] = QVariantMap{{"password", "Synthetic-Pass-2!"}, {"notes", ""}};
        m_secrets["a3"] = QVariantMap{{"password", "Synthetic-Pass-3!"}, {"notes", ""}};
    } else if (name == QLatin1String("invalidated")) {
        m_message = QStringLiteral("The vault key was invalidated (a fingerprint was added or the screen lock was removed). "
                                   "This vault can no longer be opened on this phone. Restore it from a backup.");
    } else if (name == QLatin1String("error")) {
        m_message = QStringLiteral("The vault file failed its integrity check and was not opened. Restore it from a backup.");
    } else if (name == QLatin1String("restore_ready")) {
        m_restoreCount = 3;
        m_message = QStringLiteral("Backup decrypted. Unlock to create a new vault from it.");
    }
    emit entriesChanged();
    emit changed();
}

void FakeVault::unlock() { setScenario(m_state == QLatin1String("none") ? QStringLiteral("empty") : QStringLiteral("unlocked")); }

QVariantMap FakeVault::entry(const QString &id)
{
    for (const QVariant &v : std::as_const(m_entries)) {
        QVariantMap e = v.toMap();
        if (e.value("id") == id) {
            e["autotype"] = QStringLiteral("{USERNAME}{TAB}{PASSWORD}{ENTER}");
            return e;
        }
    }
    return {};
}

QVariantMap FakeVault::reveal(const QString &id) { return m_secrets.value(id).toMap(); }

QString FakeVault::save(const QVariantMap &e)
{
    QString id = e.value("id").toString();
    if (id.isEmpty()) id = QStringLiteral("n%1").arg(m_entries.size() + 1);
    const QVariantMap r = row(id, e.value("title").toString(), e.value("username").toString(), e.value("url").toString());
    bool found = false;
    for (QVariant &v : m_entries)
        if (v.toMap().value("id") == id) { v = r; found = true; }
    if (!found) m_entries.append(r);
    m_secrets[id] = QVariantMap{{"password", e.value("password")}, {"notes", e.value("notes")}};
    emit entriesChanged();
    return QStringLiteral("ok:") + id;
}

bool FakeVault::remove(const QString &id)
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries[i].toMap().value("id") == id) { m_entries.removeAt(i); emit entriesChanged(); return true; }
    return false;
}

QString FakeVault::generatePassword(int length, bool, bool, bool, bool, bool) { return QString(length, QLatin1Char('x')).replace(2, 1, "Q"); }
QString FakeVault::generatePassphrase(int words, const QString &sep, bool, bool)
{
    QStringList w; for (int i = 0; i < words; ++i) w << QStringLiteral("word%1").arg(i + 1);
    return w.join(sep);
}
QVariantMap FakeVault::passphraseHint(const QString &p)
{
    const bool ok = p.size() >= 12;
    return {{"ok", ok}, {"label", p.size() < 8 ? "weak" : ok ? "good" : "fair"}, {"bits", p.size() * 4},
            {"reason", ok ? QString() : QStringLiteral("Use at least 12 characters or 4 words")}};
}
QString FakeVault::exportBackup(const QString &pass, const QString &confirm)
{
    if (pass != confirm) return QStringLiteral("The passphrases do not match.");
    if (pass.size() < 12) return QStringLiteral("Use at least 12 varied characters or 4 words.");
    m_backupOp = QStringLiteral("export"); m_backupStatus = QStringLiteral("done"); m_backupCount = m_entries.size();
    emit changed();
    return {};
}
QString FakeVault::importBackup(const QString &, const QString &mode)
{
    m_backupOp = QStringLiteral("import"); m_backupStatus = QStringLiteral("done"); m_backupCount = 3;
    if (mode == QLatin1String("restore")) { setScenario(QStringLiteral("restore_ready")); return {}; }
    emit changed();
    return {};
}
