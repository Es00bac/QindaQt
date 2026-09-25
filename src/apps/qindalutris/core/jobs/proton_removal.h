// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace QindaQt::QindaLutris {

class JobLog;

// AGENT-CONTRACT: "remove a user-installed build" of ADR-0275 section 2.
// Removal is refused (with one plain sentence) when:
//  - the build name is unsafe or the build is not a real directory directly
//    under the user root (a symlink is refused, never followed);
//  - the root is, or lies under, a system root -- builds there belong to
//    Portage and the app never removes them;
//  - the caller-supplied pinned set names the build. The caller (the
//    TitleRecord store) is the authority on pins; this code never guesses.
// Removal first renames the build into `<root>/.qindalutris-trash/` (atomic,
// so the build disappears from every scanner at once), then deletes the
// trashed copy. A deletion interrupted by a crash leaves only trash, which
// sweepProtonTrash() clears on the next run.
struct ProtonRemovalRequest final {
  QString userRoot;  // e.g. defaultUserCompatToolsRoot()
  QString buildName; // directory name, e.g. "GE-Proton11-7-x86_64"
  QSet<QString> pinnedBuildNames;
  QStringList systemRoots = defaultSystemRoots();

  [[nodiscard]] static QStringList defaultSystemRoots();
};

struct ProtonRemovalResult final {
  bool ok = false;
  QString message; // ONE plain sentence
};

// Pure-ish policy check (reads file metadata only). nullopt means allowed.
[[nodiscard]] std::optional<QString> checkProtonRemoval(const ProtonRemovalRequest &request);

// Checks, then removes. May take seconds for a large build: callers should
// run it off the GUI thread (it touches no QObject). log may be null.
[[nodiscard]] ProtonRemovalResult removeProtonBuild(const ProtonRemovalRequest &request,
                                                    JobLog *log = nullptr);

// Deletes leftovers of interrupted removals. Safe to call at any time.
void sweepProtonTrash(const QString &userRoot);

} // namespace QindaQt::QindaLutris
