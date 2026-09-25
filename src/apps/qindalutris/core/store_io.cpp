// SPDX-License-Identifier: GPL-3.0-or-later
#include "store_io.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>

namespace QindaQt::QindaLutris::StoreIo {

QByteArray readBoundedFile(const QString &path, qint64 maxBytes,
                           ReadStatus *status) {
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink()) {
    *status = ReadStatus::Absent;
    return {};
  }
  if (!info.isFile() || info.isSymLink() || info.size() > maxBytes) {
    *status = ReadStatus::Refused;
    return {};
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    *status = ReadStatus::Refused;
    return {};
  }
  const QByteArray bytes = file.read(maxBytes + 1);
  if (bytes.size() > maxBytes) {
    *status = ReadStatus::Refused; // grew between stat and read
    return {};
  }
  *status = bytes.isEmpty() ? ReadStatus::Absent : ReadStatus::Ok;
  return bytes;
}

bool writeAtomicJson(const QString &path, const QJsonDocument &document) {
  const QFileInfo info(path);
  QDir dir(info.absolutePath());
  if (!dir.mkpath(QStringLiteral("."))) {
    return false;
  }
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly)) {
    return false;
  }
  file.write(document.toJson(QJsonDocument::Compact));
  return file.commit();
}

QString boundedString(const QJsonValue &value, int maxChars, bool *ok) {
  *ok = false;
  if (!value.isString()) {
    return {};
  }
  const QString text = value.toString();
  if (text.size() > maxChars) {
    return {};
  }
  for (const QChar ch : text) {
    if (ch.category() == QChar::Other_Control && ch != QLatin1Char('\n')) {
      return {};
    }
  }
  *ok = true;
  return text;
}

bool isCleanLine(const QString &text, int maxChars) {
  if (text.size() > maxChars) {
    return false;
  }
  for (const QChar ch : text) {
    if (ch.category() == QChar::Other_Control || ch.isNull()) {
      return false;
    }
  }
  return true;
}

QString boundedLine(const QJsonValue &value, int maxChars, bool *ok) {
  *ok = false;
  if (!value.isString()) {
    return {};
  }
  const QString text = value.toString();
  if (!isCleanLine(text, maxChars)) {
    return {};
  }
  *ok = true;
  return text;
}

QStringList boundedLineList(const QJsonValue &value, int maxEntries,
                            int maxChars, bool *ok) {
  *ok = false;
  if (!value.isArray()) {
    return {};
  }
  const QJsonArray array = value.toArray();
  if (array.size() > maxEntries) {
    return {};
  }
  QStringList out;
  out.reserve(array.size());
  for (const QJsonValue &entry : array) {
    bool entryOk = false;
    const QString text = boundedLine(entry, maxChars, &entryOk);
    if (!entryOk) {
      return {};
    }
    out.append(text);
  }
  *ok = true;
  return out;
}

} // namespace QindaQt::QindaLutris::StoreIo
