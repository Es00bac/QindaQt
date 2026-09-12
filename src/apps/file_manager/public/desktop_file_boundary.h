// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../model/file_manager_types.h"
#include "../model/launch_intent.h"
#include "../mutation/mutation_controller.h"

#include <QObject>
#include <QString>

#include <memory>

namespace QindaQt::Apps::FileManager::Desktop {

// AGENT-CONTRACT: FileBoundary is the sole path from Desktop-owned code
// (window listing/launch and, later, the network worker's URL/job
// integration) into File Manager's local-filesystem authority. Desktop must
// never construct LocalDirectoryLister, DesktopFileLauncher, or
// LocalMutationBackend itself, and must never include File Manager's
// model/**, mutation/**, or app_shell/** headers directly; those remain
// File Manager's private implementation surface even though their C++
// visibility is technically public (see [Module
// boundaries](../../../docs/wiki/architecture/module-boundaries.md)). Every
// member is stateless and safe to call from the GUI thread only, mirroring
// the synchronous, bounded-local-I/O contract of the model classes it
// composes. It resolves and validates only local absolute paths; portal,
// network, and mount locations are out of scope and remain later slices.
class FileBoundary final {
public:
  FileBoundary() = delete;

  // Reads one local directory exactly as File Manager's own navigation does:
  // a single bounded synchronous QDir read (LocalDirectoryLister::maximumEntries
  // entries at most), never following the listing into a second directory and
  // never blocking longer than that one read. A typed ListingError (never a
  // thrown exception) reports a missing path, a non-directory path,
  // permission denial, or an unclassified failure; ListingResult::truncated
  // reports the bound being hit. Returned DirectoryEntry values carry the
  // same device/inode/size/modification-time/mode identity File Manager's
  // own mutation contract consumes, so a caller can request an operation on a
  // listed entry without a second stat.
  [[nodiscard]] static ListingResult listLocalFolder(const QString &absolutePath);

  // Validates and requests a bounded local launch exactly as File Manager's
  // own DesktopFileLauncher does: the target must exist, an existing symlink
  // is resolved once to its canonical target, the resolved target must be a
  // readable regular file, and only then is the desktop's already configured
  // default handler asked to open it. A validation failure or a declined
  // handler becomes a typed LaunchError instead of throwing, blocking, or
  // executing an arbitrary command. File Manager owns no MIME database,
  // handler list, or launched-process lifetime, and neither does this seam.
  [[nodiscard]] static LaunchResult launchLocalFile(const QString &absolutePath);

  // Composes one MutationController over the same LocalMutationBackend and
  // home-Trash root ($XDG_DATA_HOME/Trash) File Manager's own main.cpp wires,
  // so identity-checked create/rename/copy/move/Trash/restore requests behave
  // identically for Desktop-initiated operations. The returned controller is
  // GUI-thread confined, owns exactly one worker thread and at most one
  // in-flight operation, and publishes bounded progress/typed failure through
  // its existing Qt properties/signals; the caller owns its lifetime (parent
  // it, or keep the unique_ptr alive, for as long as an operation may be
  // in flight) and must destroy or reparent it before requesting a second
  // operation from a different owner. This composes existing, already-
  // accepted controller/backend wiring; it adds no new mutation policy.
  [[nodiscard]] static std::unique_ptr<MutationController>
  createLocalMutationController(QObject *parent = nullptr);
};

} // namespace QindaQt::Apps::FileManager::Desktop
