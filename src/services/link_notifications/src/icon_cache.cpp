// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_notifications/icon_cache.h>
#include <qindaqt/services/link_notifications/phone_notification.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QSaveFile>

namespace QindaQt::Link {

QString IconCache::path(const QString &hash) const
{
    return isIconHash(hash) ? m_dir + QLatin1Char('/') + hash + QStringLiteral(".png") : QString();
}

bool IconCache::has(const QString &hash) const
{
    const QString p = path(hash);
    if (p.isEmpty() || !QFile::exists(p)) return false;
    QFile f(p);
    if (f.open(QIODevice::ReadOnly)) f.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
    return true;
}

void IconCache::evictToFit() const
{
    QFileInfoList files = QDir(m_dir).entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Time);  // newest first
    while (files.size() >= m_max && !files.isEmpty()) QFile::remove(files.takeLast().absoluteFilePath());
}

bool IconCache::store(const QString &hash, const QByteArray &png)
{
    const QString p = path(hash);
    if (p.isEmpty()) return false;
    if (QFile::exists(p)) return true;
    static const QByteArray signature("\x89PNG\r\n\x1a\n", 8);
    if (png.size() <= signature.size() || png.size() > NotifyLimits::kIconBytes || !png.startsWith(signature)) return false;
    if (QCryptographicHash::hash(png, QCryptographicHash::Sha256).toHex() != hash.toLatin1()) return false;
    if (!QDir().mkpath(m_dir)) return false;
    evictToFit();
    QFile::setPermissions(m_dir, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    QSaveFile f(p);
    if (!f.open(QIODevice::WriteOnly) || f.write(png) != png.size()) return false;
    return f.commit();
}

} // namespace QindaQt::Link
