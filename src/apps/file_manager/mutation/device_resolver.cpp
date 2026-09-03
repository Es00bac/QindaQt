// SPDX-License-Identifier: GPL-3.0-or-later
#include "device_resolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <sys/stat.h>

namespace QindaQt::Apps::FileManager {

std::optional<quint64>
LocalDeviceResolver::deviceForPath(const QString &path) const {
  QString candidate = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  struct stat status {};
  while (::lstat(QFile::encodeName(candidate).constData(), &status) != 0) {
    const QString parent = QFileInfo(candidate).absolutePath();
    if (parent == candidate) {
      return std::nullopt;
    }
    candidate = parent;
  }
  return static_cast<quint64>(status.st_dev);
}

} // namespace QindaQt::Apps::FileManager
