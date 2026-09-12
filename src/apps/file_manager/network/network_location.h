// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QUrl>
#include <QVector>

#include <optional>

namespace QindaQt::Apps::FileManager {

enum class LocationScheme {
  // Anything classify() does not recognize as a canonical smb/sftp scheme,
  // including every ordinary local absolute path: NavigationController's
  // existing local-path pipeline (QDir::cleanPath, LocalDirectoryLister) is
  // unchanged for this case.
  Local,
  Smb,
  Sftp,
};

struct NetworkBreadcrumbSegment final {
  QString name;
  QUrl url;

  friend bool operator==(const NetworkBreadcrumbSegment &,
                         const NetworkBreadcrumbSegment &) = default;
};

// AGENT-CONTRACT: the sole allowlist/canonicalization gate for a browsable
// network location (S5). Only lower-cased "smb" and "sftp" schemes with a
// non-empty host and no embedded userinfo are accepted -- this module never
// carries a credential in a location string, matching the "no credential
// store" boundary. A canonicalized URL never contains a "." or ".." path
// segment, so two spellings of the same remote folder always compare equal
// and no path can escape the authority root. classify() performs no I/O and
// never touches the filesystem or network; it is a pure text classification
// used by both NavigationController (routing) and the production KIO
// adapter (its own independent policy check).
class NetworkLocation final {
public:
  // Mirrors LocalDirectoryLister::maximumEntries and bounds a remote
  // diagnostic string so a hostile/verbose server response cannot grow a
  // status message without limit.
  static constexpr qsizetype maximumEntries = 20000;
  static constexpr qsizetype maximumDiagnosticLength = 4096;

  [[nodiscard]] static LocationScheme classify(const QString &location);
  [[nodiscard]] static bool isSupportedScheme(const QUrl &url);
  [[nodiscard]] static QString schemeName(LocationScheme scheme);

  // Returns nullopt for anything classify() does not report as Smb/Sftp, or
  // a malformed location (empty/invalid host, embedded credentials, or a
  // ".." segment attempting to escape the authority root).
  [[nodiscard]] static std::optional<QUrl> canonicalize(const QString &location);

  // One level up, or nullopt already at the bare authority root
  // (e.g. smb://server) -- mirrors NavigationHistory::parentOf.
  [[nodiscard]] static std::optional<QUrl> parentOf(const QUrl &canonicalUrl);
  // Root-first cumulative segments -- mirrors NavigationHistory::breadcrumbFor.
  [[nodiscard]] static QVector<NetworkBreadcrumbSegment>
  breadcrumbFor(const QUrl &canonicalUrl);
  // Builds the canonical child URL for one plain entry name (never containing
  // '/') under a canonical parent URL.
  [[nodiscard]] static QUrl childUrl(const QUrl &canonicalParent, const QString &name);

  [[nodiscard]] static QString boundedDiagnostic(const QString &diagnostic);
};

} // namespace QindaQt::Apps::FileManager
