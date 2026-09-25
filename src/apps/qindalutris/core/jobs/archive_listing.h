// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QByteArray>
#include <QChar>
#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: the first path-escape gate for Proton archives (ADR-0275
// section 2). The job lists the archive with
//   LC_ALL=C tar --list --verbose --numeric-owner --quoting-style=c --gzip
// (tarListArguments()) and extracts only when validateArchiveListing()
// accepts. The C quoting makes every name and link target unambiguous (a
// name may contain " -> ", spaces or newlines); numeric owners keep
// attacker-chosen user names out of the parse.
// AGENT-GUARD: this listing check is the FIRST of two gates; it decides
// before anything is written, from names and link targets only. It refuses:
//  - absolute names, any `..` segment, or more than one top-level name;
//  - a top-level entry that is not a directory;
//  - an absolute symlink target, or one that -- resolved physically through
//    the other links in the listing (link_resolution.h) -- leaves the top
//    folder;
//  - a hard link whose target is outside the top folder or passes through
//    a symlink;
//  - devices, FIFOs and every other special type; setuid/setgid modes;
//  - a name listed twice, or an entry beneath a non-directory entry
//    (the "symlink, then a directory of the same name" overwrite trick).
// It cannot see what tar actually builds, so the SECOND gate,
// verifyStagedBuild() (staged_tree_check.h), re-resolves every link in the
// real extracted tree before the build is committed. GNU tar's own
// extraction defences are a third line only.
// Real GE-Proton builds pass: all ~1,900 of their symlinks are relative and
// stay inside the build (checked on GE-Proton11-6-x86_64, 2026-09-25).

struct ArchiveEntry final {
  QChar type;        // '-', 'd', 'l', 'h', 'c', 'b', 'p', ...
  QString mode;      // "rwxr-xr-x" (nine characters after the type)
  QString name;      // as stored, decoded from C quoting
  QString linkTarget; // symlink or hard-link target; empty otherwise
};

[[nodiscard]] QStringList tarListArguments(const QString &archivePath);

// One line of the listing above; nullopt when it does not parse.
[[nodiscard]] std::optional<ArchiveEntry> parseArchiveListingLine(const QByteArray &line);

struct ArchiveListingVerdict final {
  bool ok = false;
  QString topLevel;
  QString reason; // for the details log when !ok
};

inline constexpr qsizetype kMaxArchiveEntries = 200000;

// `standardError` is tar's diagnostics for the same listing: a tar that had
// to strip leading '/' or '../' from a name is refusing-grade by itself.
[[nodiscard]] ArchiveListingVerdict validateArchiveListing(
    const QByteArray &verboseListing, const QByteArray &standardError = {},
    qsizetype maxEntries = kMaxArchiveEntries);

} // namespace QindaQt::QindaLutris
