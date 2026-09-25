// SPDX-License-Identifier: GPL-3.0-or-later
#include "store_io.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSaveFile>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::QindaLutris::StoreIo {

QByteArray readBoundedFile(const QString &path, qint64 maxBytes,
                           ReadStatus *status) {
  // AGENT-GUARD: O_NOFOLLOW + fstat on the OPENED descriptor. A stat-then-
  // open check could be raced by swapping in a symlink between the two;
  // the kernel refuses a trailing symlink here (ELOOP) and the type and
  // size checks apply to exactly the file that will be read. O_NONBLOCK
  // keeps a planted FIFO from hanging the open; fstat then refuses it.
  const QByteArray native = QFile::encodeName(path);
  const int fd = ::open(native.constData(),
                        O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
  if (fd < 0) {
    *status = errno == ENOENT ? ReadStatus::Absent : ReadStatus::Refused;
    return {};
  }
  struct stat info {};
  if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)
      || qint64(info.st_size) > maxBytes) {
    ::close(fd);
    *status = ReadStatus::Refused;
    return {};
  }
  QFile file;
  if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    ::close(fd);
    *status = ReadStatus::Refused;
    return {};
  }
  const QByteArray bytes = file.read(maxBytes + 1);
  if (bytes.size() > maxBytes) {
    *status = ReadStatus::Refused; // grew between fstat and read
    return {};
  }
  *status = bytes.isEmpty() ? ReadStatus::Absent : ReadStatus::Ok;
  return bytes;
}

bool writeAtomicJson(const QString &path, const QJsonDocument &document) {
  // AGENT-GUARD: QSaveFile deliberately writes THROUGH a symlinked
  // destination (it resolves the link and replaces the target). A planted
  // titles-v1.json -> ~/anything link would then overwrite the link's
  // target, so a destination that exists and is not a regular file -- a
  // symlink included -- is refused before QSaveFile ever sees it.
  const QByteArray native = QFile::encodeName(path);
  struct stat existing {};
  if (::lstat(native.constData(), &existing) == 0
      && !S_ISREG(existing.st_mode)) {
    return false;
  }
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
