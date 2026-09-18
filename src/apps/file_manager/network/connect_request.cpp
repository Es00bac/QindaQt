// SPDX-License-Identifier: GPL-3.0-or-later
#include "connect_request.h"

#include "network_location.h"

#include <QStringList>
#include <QUrl>

namespace QindaQt::Apps::FileManager {
namespace {

[[nodiscard]] ConnectRequestResult refusal(const ConnectRequestError error,
                                           const QString &message) {
  return {.record = {}, .error = error, .message = message};
}

// A host is the one field that becomes URL authority, so it is checked
// before QUrl ever sees it: a '@' would create userinfo, a '/' would create
// a path, and whitespace or a control character would make QUrl's tolerant
// parse produce something the user did not type.
[[nodiscard]] bool hostLooksWellFormed(const QString &host) {
  for (const QChar character : host) {
    if (character.isSpace() || character.category() == QChar::Other_Control ||
        character == QLatin1Char('@') || character == QLatin1Char('/') ||
        character == QLatin1Char('\\') || character == QLatin1Char('?') ||
        character == QLatin1Char('#')) {
      return false;
    }
  }
  return true;
}

// "/mnt/storage" on "qinda" reads as "storage on qinda"; a bare authority
// reads as its host. The name is only a label -- it never routes.
[[nodiscard]] QString derivedName(const QUrl &canonical) {
  const QStringList segments =
      canonical.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (segments.isEmpty()) {
    return canonical.host();
  }
  return QStringLiteral("%1 on %2").arg(segments.constLast(), canonical.host());
}

} // namespace

QStringList connectableSchemes() {
  return {QStringLiteral("sftp"), QStringLiteral("smb")};
}

ConnectRequestResult buildNetworkLocation(const ConnectRequest &request) {
  const QString scheme = request.scheme.trimmed().toLower();
  if (!connectableSchemes().contains(scheme)) {
    return refusal(ConnectRequestError::UnsupportedScheme,
                   QStringLiteral("Choose either SFTP or Windows sharing (SMB)."));
  }
  const QString host = request.host.trimmed();
  if (host.isEmpty()) {
    return refusal(ConnectRequestError::MissingHost,
                   QStringLiteral("Enter the server's name or address."));
  }
  if (!hostLooksWellFormed(host)) {
    return refusal(
        ConnectRequestError::InvalidHost,
        QStringLiteral("The server name may not contain a space, a slash, or an "
                       "\"@\". Sign-in details are asked for when connecting."));
  }

  int port = -1;
  const QString portText = request.port.trimmed();
  if (!portText.isEmpty()) {
    bool parsed = false;
    port = portText.toInt(&parsed);
    if (!parsed || port < 1 || port > 65535) {
      return refusal(ConnectRequestError::InvalidPort,
                     QStringLiteral("The port must be a number between 1 and 65535."));
    }
  }

  QString remotePath = request.remotePath.trimmed();
  if (!remotePath.isEmpty() && !remotePath.startsWith(QLatin1Char('/'))) {
    remotePath.prepend(QLatin1Char('/'));
  }

  QUrl assembled;
  assembled.setScheme(scheme);
  assembled.setHost(host);
  if (port > 0) {
    assembled.setPort(port);
  }
  assembled.setPath(remotePath);
  if (!assembled.isValid() || assembled.host().isEmpty()) {
    return refusal(ConnectRequestError::InvalidHost,
                   QStringLiteral("That server name or address is not usable."));
  }
  // AGENT-GUARD: canonicalize() is the allowlist for everything that will be
  // browsed or stored. Refusing here -- not repairing -- is what keeps a
  // "..", an unsupported scheme, or embedded userinfo out of the inventory.
  const auto canonical = NetworkLocation::canonicalize(assembled.toString());
  if (!canonical.has_value()) {
    return refusal(ConnectRequestError::InvalidPath,
                   QStringLiteral("That address cannot be opened. Check the folder "
                                  "path: \"..\" is not allowed."));
  }
  if (canonical->toString().size() > NetworkLocationsStore::maximumUrlLength) {
    return refusal(ConnectRequestError::InvalidPath,
                   QStringLiteral("That address is too long to save."));
  }

  QString name = request.displayName.trimmed();
  if (name.isEmpty()) {
    name = derivedName(*canonical);
  }
  if (name.isEmpty() || name.size() > NetworkLocationsStore::maximumNameLength ||
      !name.isValidUtf16() || name.contains(QChar::Null)) {
    return refusal(ConnectRequestError::InvalidName,
                   QStringLiteral("Give the location a shorter name."));
  }

  if (request.mountAtLogin && scheme != QLatin1String("sftp")) {
    return refusal(ConnectRequestError::MountUnsupported,
                   QStringLiteral("Only SFTP locations can be mounted at login."));
  }

  NetworkLocationRecord record;
  record.id = NetworkLocationsStore::identityFor(*canonical);
  record.name = name;
  record.url = *canonical;
  record.showInPlaces = request.showInPlaces;
  record.mountAtLogin = request.mountAtLogin;
  return {.record = record, .error = ConnectRequestError::None, .message = {}};
}

} // namespace QindaQt::Apps::FileManager
