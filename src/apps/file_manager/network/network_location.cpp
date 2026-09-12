// SPDX-License-Identifier: GPL-3.0-or-later
#include "network_location.h"

#include <QStringList>

namespace QindaQt::Apps::FileManager {

namespace {

[[nodiscard]] QStringList pathSegments(const QUrl &url) {
  return url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
}

[[nodiscard]] QString pathFromSegments(const QStringList &segments) {
  return segments.isEmpty() ? QString()
                            : QLatin1Char('/') + segments.join(QLatin1Char('/'));
}

} // namespace

LocationScheme NetworkLocation::classify(const QString &location) {
  const QString trimmed = location.trimmed();
  const qsizetype schemeEnd = trimmed.indexOf(QStringLiteral("://"));
  if (schemeEnd <= 0) {
    return LocationScheme::Local;
  }
  const QString scheme = trimmed.left(schemeEnd).toLower();
  if (scheme == QStringLiteral("smb")) {
    return LocationScheme::Smb;
  }
  if (scheme == QStringLiteral("sftp")) {
    return LocationScheme::Sftp;
  }
  return LocationScheme::Local;
}

bool NetworkLocation::isSupportedScheme(const QUrl &url) {
  return url.scheme() == QStringLiteral("smb") ||
         url.scheme() == QStringLiteral("sftp");
}

QString NetworkLocation::schemeName(LocationScheme scheme) {
  switch (scheme) {
  case LocationScheme::Smb:
    return QStringLiteral("smb");
  case LocationScheme::Sftp:
    return QStringLiteral("sftp");
  case LocationScheme::Local:
    break;
  }
  return QString();
}

std::optional<QUrl> NetworkLocation::canonicalize(const QString &location) {
  const LocationScheme scheme = classify(location);
  if (scheme == LocationScheme::Local) {
    return std::nullopt;
  }
  QUrl url(location.trimmed(), QUrl::TolerantMode);
  if (!url.isValid() || url.host().isEmpty()) {
    return std::nullopt;
  }
  // AGENT-GUARD: no credential ever travels through a location string; a
  // pasted "smb://user:pass@host/share" is refused outright rather than
  // silently dropping the credential and connecting anonymously.
  if (!url.userName().isEmpty() || !url.password().isEmpty()) {
    return std::nullopt;
  }
  url.setScheme(schemeName(scheme));
  // Scheme and host are case-insensitive (RFC 3986); normalizing both here
  // is what makes two spellings of the same remote folder compare equal.
  url.setHost(url.host().toLower());
  url.setFragment({});
  url.setQuery({});
  QStringList segments;
  for (const QString &segment : pathSegments(url)) {
    if (segment == QLatin1String(".")) {
      continue;
    }
    if (segment == QLatin1String("..")) {
      // Never escape the authority root.
      return std::nullopt;
    }
    segments.append(segment);
  }
  url.setPath(pathFromSegments(segments));
  return url;
}

std::optional<QUrl> NetworkLocation::parentOf(const QUrl &canonicalUrl) {
  QStringList segments = pathSegments(canonicalUrl);
  if (segments.isEmpty()) {
    return std::nullopt;
  }
  segments.removeLast();
  QUrl parent = canonicalUrl;
  parent.setPath(pathFromSegments(segments));
  return parent;
}

QVector<NetworkBreadcrumbSegment>
NetworkLocation::breadcrumbFor(const QUrl &canonicalUrl) {
  QVector<NetworkBreadcrumbSegment> segments;
  QUrl root = canonicalUrl;
  root.setPath({});
  segments.append({root.toString(), root});
  QStringList accumulated;
  for (const QString &part : pathSegments(canonicalUrl)) {
    accumulated.append(part);
    QUrl url = root;
    url.setPath(pathFromSegments(accumulated));
    segments.append({part, url});
  }
  return segments;
}

QUrl NetworkLocation::childUrl(const QUrl &canonicalParent, const QString &name) {
  QUrl child = canonicalParent;
  QStringList segments = pathSegments(canonicalParent);
  segments.append(name);
  child.setPath(pathFromSegments(segments));
  return child;
}

QString NetworkLocation::boundedDiagnostic(const QString &diagnostic) {
  return diagnostic.left(maximumDiagnosticLength);
}

} // namespace QindaQt::Apps::FileManager
