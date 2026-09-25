// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <QVector>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: GE-Proton releases as ADR-0275 section 2 downloads them:
// only from GloriousEggroll/proton-ge-custom GitHub releases, only with a
// matching `.sha512sum` asset. Pure parsing; no I/O except sha512HexOfFile.
//
// Asset naming (verified against the live API on 2026-09-25):
//   GE-Proton11-6-x86_64.tar.gz      the build; its single top-level
//                                    directory is `GE-Proton11-6-x86_64`
//   GE-Proton11-6-x86_64.sha512sum   "<128 hex>  GE-Proton11-6-x86_64.tar.gz"
// Older releases name the tarball `GE-Proton9-20.tar.gz` (no architecture);
// those are accepted for x86_64 only.
//
// AGENT-CONTRACT (with the Proton catalog): `toolName` is the tarball name
// without `.tar.gz`; it is the directory name under compatibilitytools.d and
// therefore the build identity the catalog and TitleRecord pin. Portage's
// app-emulation/ge-proton-bin installs the same directory names.
struct GeProtonRelease final {
  QString tagName;      // "GE-Proton11-6"
  QString toolName;     // "GE-Proton11-6-x86_64"
  QString tarballName;  // "GE-Proton11-6-x86_64.tar.gz"
  QUrl tarballUrl;
  qint64 tarballBytes = -1;
  QString checksumName; // "GE-Proton11-6-x86_64.sha512sum"
  QUrl checksumUrl;
  QDateTime publishedAt;
  bool prerelease = false;

  friend bool operator==(const GeProtonRelease &, const GeProtonRelease &) = default;
};

struct GeProtonReleaseList final {
  bool ok = false;
  QString error; // plain reason when !ok
  QVector<GeProtonRelease> releases; // API order (newest first)
};

inline constexpr qsizetype kMaxReleaseDocumentBytes = 8 * 1024 * 1024;
inline constexpr int kMaxReleases = 100;

// https://api.github.com/repos/GloriousEggroll/proton-ge-custom/releases?per_page=N
// (N clamped to 1..100).
[[nodiscard]] QUrl geProtonReleasesApiUrl(int perPage);

// Parses the GitHub releases API document. Drafts, releases without a
// matching tarball + checksum pair, unsafe names, and assets hosted off the
// download allowlist are skipped (never guessed). A document that is not a
// JSON array, or oversized, is refused whole.
[[nodiscard]] GeProtonReleaseList parseGeProtonReleases(
    const QByteArray &json, const QString &architecture = QStringLiteral("x86_64"));

// A directory name safe to create under a compatibility-tools root:
// [A-Za-z0-9][A-Za-z0-9._+-]{0,127}, never "." or "..".
[[nodiscard]] bool isSafeToolName(const QString &name);

// Finds the lower-case hex SHA-512 for fileName in sha512sum output
// ("<hex>  <name>" or "<hex> *<name>"); nullopt when absent or malformed.
[[nodiscard]] std::optional<QByteArray> parseSha512SumFile(const QByteArray &content,
                                                           const QString &fileName);

// Streams the file through QCryptographicHash; nullopt when unreadable.
[[nodiscard]] std::optional<QByteArray> sha512HexOfFile(const QString &path);

// The verdict on `tar --list` output for an archive that must unpack into
// exactly one top-level directory. AGENT-GUARD: this is the path-escape
// defence -- any absolute name, any `..` segment, or a second top-level
// name refuses the whole archive before extraction starts.
struct ArchiveListingVerdict final {
  bool ok = false;
  QString topLevel;
  QString reason;
};
inline constexpr qsizetype kMaxArchiveEntries = 200000;
[[nodiscard]] ArchiveListingVerdict validateSingleTopLevelListing(
    const QByteArray &listing, qsizetype maxEntries = kMaxArchiveEntries);

} // namespace QindaQt::QindaLutris
