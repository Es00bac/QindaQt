// SPDX-License-Identifier: GPL-3.0-or-later
#include "local_preview.h"
#include <QBuffer>
#include <QFile>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace QindaQt::Apps::FileManager {
namespace {
bool matches(const struct stat &st, const DirectoryEntry &entry) {
  return S_ISREG(st.st_mode) &&
         static_cast<quint64>(st.st_dev) == entry.device &&
         static_cast<quint64>(st.st_ino) == entry.inode &&
         st.st_size == entry.identitySize &&
         static_cast<quint32>(st.st_mode) == entry.mode &&
         st.st_mtim.tv_sec * 1000000000LL + st.st_mtim.tv_nsec ==
             entry.modifiedNanoseconds;
}
} // namespace
bool previewIdentityMatches(const DirectoryEntry &entry) {
  struct stat current{};
  return ::lstat(QFile::encodeName(entry.absolutePath).constData(), &current) ==
             0 &&
         matches(current, entry);
}
QImage LocalPreviewDecoder::decode(const DirectoryEntry &entry,
                                   const std::atomic_bool &cancelled) const {
  if (cancelled || entry.isDirectory || entry.isSymlink ||
      entry.identitySize <= 0 || entry.identitySize > maximumBytes)
    return {};
  const int fd = ::open(QFile::encodeName(entry.absolutePath).constData(),
                        O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0)
    return {};
  QFile file;
  if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
    ::close(fd);
    return {};
  }
  struct stat before{};
  if (::fstat(fd, &before) != 0 || !matches(before, entry) || cancelled)
    return {};
  // Pin a bounded byte snapshot as well as the descriptor. A file that grows
  // while a codec reads must not evade the 32 MiB admission limit.
  QByteArray contents = file.read(entry.identitySize + 1);
  if (contents.size() != entry.identitySize || cancelled)
    return {};
  QBuffer bytes(&contents);
  if (!bytes.open(QIODevice::ReadOnly))
    return {};
  QImageReader reader(&bytes);
  // AGENT-GUARD: Only bounded raster decoders enter previews. SVG and animated
  // documents retain catalog icons; see ADR-0100 for the resource contract.
  const QByteArray format = reader.format().toLower();
  if (format != "png" && format != "jpeg" && format != "jpg" &&
      format != "bmp" && format != "webp")
    return {};
  const QSize dimensions = reader.size();
  if (!dimensions.isValid() ||
      static_cast<qint64>(dimensions.width()) * dimensions.height() >
          maximumPixels)
    return {};
  reader.setAutoTransform(true);
  reader.setScaledSize(dimensions.scaled(192, 192, Qt::KeepAspectRatio));
  if (cancelled)
    return {};
  QImage result = reader.read();
  if (result.isNull())
    return {};
  struct stat after{}, pathNow{};
  if (cancelled || ::fstat(fd, &after) != 0 || !matches(after, entry) ||
      ::lstat(QFile::encodeName(entry.absolutePath).constData(), &pathNow) !=
          0 ||
      !matches(pathNow, entry))
    return {};
  return result.scaled(192, 192, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
QString entryIconName(const DirectoryEntry &entry) {
  if (entry.isDirectory)
    return QStringLiteral("folder");
  const auto mime = QMimeDatabase().mimeTypeForFile(
      entry.name, QMimeDatabase::MatchExtension);
  if (mime.name().startsWith(QLatin1String("image/")))
    return QStringLiteral("image-x-generic");
  if (mime.name().startsWith(QLatin1String("audio/")))
    return QStringLiteral("audio-x-generic");
  if (mime.name().startsWith(QLatin1String("video/")))
    return QStringLiteral("video-x-generic");
  if (mime.name().contains(QLatin1String("zip")) ||
      mime.name().contains(QLatin1String("tar")) ||
      mime.name().contains(QLatin1String("compressed")))
    return QStringLiteral("application-x-archive");
  if (mime.name() == QLatin1String("application/pdf"))
    return QStringLiteral("application-pdf");
  if (mime.name().contains(QLatin1String("script")) ||
      mime.name().contains(QLatin1String("src")) ||
      mime.name() == QLatin1String("application/json"))
    return QStringLiteral("text-x-script");
  if (mime.name().startsWith(QLatin1String("text/")))
    return QStringLiteral("text-x-generic");
  return QStringLiteral("application-octet-stream");
}
QString previewUrl(const DirectoryEntry &entry, quint64 generation) {
  if (entry.isDirectory || entry.isSymlink || entry.identitySize <= 0 ||
      entry.identitySize > LocalPreviewDecoder::maximumBytes ||
      entryIconName(entry) != QLatin1String("image-x-generic"))
    return {};
  const QJsonObject value{{"path", entry.absolutePath},
                          {"device", QString::number(entry.device)},
                          {"inode", QString::number(entry.inode)},
                          {"size", QString::number(entry.identitySize)},
                          {"mtime", QString::number(entry.modifiedNanoseconds)},
                          {"mode", QString::number(entry.mode)}};
  return QStringLiteral("image://previews/%1/%2")
      .arg(generation)
      .arg(QString::fromLatin1(QJsonDocument(value)
                                   .toJson(QJsonDocument::Compact)
                                   .toBase64(QByteArray::Base64UrlEncoding |
                                             QByteArray::OmitTrailingEquals)));
}
} // namespace QindaQt::Apps::FileManager
