// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_input/restore_token_store.h>
#include <qindaqt/services/link_transport/private_dir.h>

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QSaveFile>

namespace QindaQt::Link {
namespace {
bool plausible(const QString &t) { return !t.isEmpty() && t.size() <= 256 && !t.contains(QLatin1Char('\n')); }
}

QString RestoreTokenStore::load() const
{
    if (m_path.isEmpty()) return {};
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    // A token readable by others is treated as compromised and ignored.
    if (f.permissions() & (QFileDevice::ReadGroup | QFileDevice::ReadOther | QFileDevice::WriteGroup | QFileDevice::WriteOther)) return {};
    const QString t = QString::fromUtf8(f.read(512)).trimmed();
    return plausible(t) ? t : QString();
}

bool RestoreTokenStore::save(const QString &token) const
{
    if (m_path.isEmpty() || !plausible(token)) return false;
    if (!ensurePrivateDirectory(QFileInfo(m_path).absolutePath())) return false;
    QSaveFile f(m_path);
    f.setDirectWriteFallback(false);
    if (!f.open(QIODevice::WriteOnly)) return false;
    if (!f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) return false;
    f.write(token.toUtf8());
    f.write("\n");
    return f.commit();
}

void RestoreTokenStore::clear() const
{
    if (!m_path.isEmpty()) QFile::remove(m_path);
}

} // namespace QindaQt::Link
