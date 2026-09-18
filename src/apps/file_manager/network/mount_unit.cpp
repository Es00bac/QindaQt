// SPDX-License-Identifier: GPL-3.0-or-later
#include "mount_unit.h"

#include <QDir>
#include <QStringList>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] MountUnitResult refusal(const MountUnitError error,
                                      const QString &message) {
  return {.unitName = {},
          .contents = {},
          .mountPoint = {},
          .error = error,
          .message = message};
}

// systemd keeps [a-zA-Z0-9:_.] verbatim, turns '/' into '-', and writes
// everything else as \xNN. A leading '.' is escaped so a unit never starts
// with one.
[[nodiscard]] bool isVerbatim(const QChar character) {
  return (character >= QLatin1Char('a') && character <= QLatin1Char('z')) ||
         (character >= QLatin1Char('A') && character <= QLatin1Char('Z')) ||
         (character >= QLatin1Char('0') && character <= QLatin1Char('9')) ||
         character == QLatin1Char(':') || character == QLatin1Char('_') ||
         character == QLatin1Char('.');
}

[[nodiscard]] QString escapeByte(const char byte) {
  return QStringLiteral("\\x%1").arg(static_cast<quint8>(byte), 2, 16,
                                     QLatin1Char('0'));
}

} // namespace

QString MountUnit::escapePath(const QString &absolutePath) {
  const QString cleaned = QDir::cleanPath(absolutePath);
  const QStringList segments = cleaned.split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (segments.isEmpty()) {
    // systemd spells the root mount "-".
    return QStringLiteral("-");
  }
  const QString joined = segments.join(QLatin1Char('/'));
  QString escaped;
  bool first = true;
  for (const QChar character : joined) {
    if (character == QLatin1Char('/')) {
      escaped.append(QLatin1Char('-'));
      first = false;
      continue;
    }
    if (isVerbatim(character) && !(first && character == QLatin1Char('.'))) {
      escaped.append(character);
      first = false;
      continue;
    }
    const QByteArray utf8 = QString(character).toUtf8();
    for (const char byte : utf8) {
      escaped.append(escapeByte(byte));
    }
    first = false;
  }
  return escaped;
}

QString MountUnit::directoryNameFor(const QString &locationName) {
  QString directory;
  for (const QChar character : locationName.trimmed()) {
    // AGENT-GUARD: a separator, a NUL, or a control character here would let
    // a location name choose its own mount point anywhere in the tree.
    if (character == QLatin1Char('/') || character == QLatin1Char('\\') ||
        character == QChar::Null || character.category() == QChar::Other_Control) {
      continue;
    }
    directory.append(character);
  }
  directory = directory.trimmed();
  while (directory.startsWith(QLatin1Char('.'))) {
    directory.remove(0, 1);
    directory = directory.trimmed();
  }
  if (directory == QLatin1String("..") || directory.size() > 128) {
    return {};
  }
  return directory;
}

MountUnitResult MountUnit::build(const NetworkLocationRecord &location,
                                 const QString &homeDirectory) {
  if (location.url.scheme() != QLatin1String("sftp")) {
    return refusal(MountUnitError::UnsupportedScheme,
                   QStringLiteral("Only SFTP locations can be mounted at login."));
  }
  if (!QDir::isAbsolutePath(homeDirectory)) {
    return refusal(MountUnitError::UnusableHome,
                   QStringLiteral("The home directory could not be resolved."));
  }
  const QString directory = directoryNameFor(location.name);
  if (directory.isEmpty()) {
    return refusal(
        MountUnitError::UnusableName,
        QStringLiteral("Give the location a name that can be a folder name."));
  }
  const QString mountPoint =
      QDir::cleanPath(QStringLiteral("%1/%2/%3")
                          .arg(homeDirectory,
                               QString::fromLatin1(parentDirectoryName), directory));

  const QString remotePath =
      location.url.path().isEmpty() ? QStringLiteral("/") : location.url.path();
  const QString what = QStringLiteral("%1:%2").arg(location.url.host(), remotePath);
  QStringList options{QStringLiteral("_netdev"), QStringLiteral("reconnect"),
                      QStringLiteral("ServerAliveInterval=15"),
                      QStringLiteral("ServerAliveCountMax=3"),
                      QStringLiteral("idmap=user")};
  if (location.url.port() > 0) {
    options.append(QStringLiteral("port=%1").arg(location.url.port()));
  }

  // AGENT-NOTE: no credential appears here and none can: the location URL is
  // userinfo-free by construction, so sshfs authenticates exactly as `ssh
  // <host>` does, through ~/.ssh/config and the agent (ADR-0196).
  const QString contents =
      QStringLiteral("# Written by QindaQt File Manager. Edits are overwritten\n"
                     "# when the location's \"Mount at login\" setting changes.\n"
                     "[Unit]\n"
                     "Description=QindaQt network location %1\n"
                     "Documentation=man:sshfs(1)\n"
                     "After=network-online.target\n"
                     "\n"
                     "[Mount]\n"
                     "What=%2\n"
                     "Where=%3\n"
                     "Type=fuse.sshfs\n"
                     "Options=%4\n"
                     "TimeoutSec=30\n"
                     "\n"
                     "[Install]\n"
                     "WantedBy=default.target\n")
          .arg(location.name, what, mountPoint, options.join(QLatin1Char(',')));

  return {.unitName = escapePath(mountPoint) + QStringLiteral(".mount"),
          .contents = contents,
          .mountPoint = mountPoint,
          .error = MountUnitError::None,
          .message = {}};
}

} // namespace QindaQt::Apps::FileManager
