// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_templates.h"

#include "local_directory_lister.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QVariantMap>

#include <algorithm>
#include <utility>

namespace QindaQt::Apps::FileManager {

FileTemplates::FileTemplates(QString directory, QObject *parent)
    : QObject(parent), m_directory(std::move(directory)) {}

QString FileTemplates::userTemplatesDirectory() {
  const QString directory =
      QStandardPaths::writableLocation(QStandardPaths::TemplatesLocation);
  return QDir::cleanPath(directory) == QDir::cleanPath(QDir::homePath()) ? QString() : directory;
}

void FileTemplates::refresh() {
  QVariantList next;
  if (!m_directory.isEmpty()) {
    ListingResult listing = LocalDirectoryLister().list(m_directory);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(listing.entries.begin(), listing.entries.end(),
                     [&collator](const DirectoryEntry &left, const DirectoryEntry &right) {
                       return collator.compare(left.name, right.name) < 0;
                     });
    for (const DirectoryEntry &entry : std::as_const(listing.entries)) {
      if (entry.isDirectory || entry.isSymlink || entry.isHidden) {
        continue;
      }
      if (next.size() >= maximumTemplates) {
        break;
      }
      const QString stem = QFileInfo(entry.name).completeBaseName();
      next.append(QVariantMap{{QStringLiteral("name"), stem.isEmpty() ? entry.name : stem},
                              {QStringLiteral("fileName"), entry.name},
                              {QStringLiteral("path"), entry.absolutePath}});
    }
  }
  if (next != m_templates) {
    m_templates = next;
    emit templatesChanged();
  }
}

} // namespace QindaQt::Apps::FileManager
