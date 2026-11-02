// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/link_transport/private_dir.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

#include <sys/stat.h>

namespace QindaQt::Link {

bool ensurePrivateDirectory(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        if (!QDir().mkpath(info.absolutePath())) {
            return false;
        }
        if (::mkdir(QFile::encodeName(info.absoluteFilePath()).constData(), 0700) != 0 && !QFileInfo(path).isDir()) {
            return false;
        }
    }
    return QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

} // namespace QindaQt::Link
