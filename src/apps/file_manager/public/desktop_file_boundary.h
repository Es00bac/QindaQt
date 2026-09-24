// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "../model/clipboard_controller.h"
#include "../model/file_manager_types.h"
#include "../model/launch_intent.h"
#include "../model/reveal_request.h"
#include "../mutation/mutation_controller.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QClipboard;

namespace QindaQt::Apps::FileManager::Desktop {

// Typed outcome of FileBoundary::openLocalFolder. `canonicalPath` is the exact
// directory handed to File Manager when ok().
enum class FolderOpenError {
  None,
  // Missing, dangling, or not an absolute local path.
  NotFound,
  // The path no longer names the file-system object the listing reported.
  Replaced,
  // The canonical target is not a directory.
  NotDirectory,
  // The directory cannot be read or entered.
  Unreadable,
  // No absolute, executable qindaqt-file-manager candidate exists.
  NotInstalled,
  // The candidate process could not be started.
  LaunchRefused,
  // The requested File Manager action is not an isRevealAction() id.
  UnsupportedAction,
};

struct FolderOpenResult final {
  FolderOpenError error = FolderOpenError::None;
  QString diagnostic;
  QString canonicalPath;

  [[nodiscard]] bool ok() const { return error == FolderOpenError::None; }
};

// Device and inode of one DirectoryEntry exactly as listLocalFolder reported
// it: lstat of the listed path, so a symlink entry is identified by the link.
struct ListedIdentity final {
  quint64 device = 0;
  quint64 inode = 0;
};

// Starts `program` detached with `arguments` as literal argv elements.
using ProcessStarter =
    std::function<bool(const QString &program, const QStringList &arguments)>;

// AGENT-CONTRACT: FileBoundary is the sole path from Desktop-owned code
// (window listing/launch/folder open/Get Info and, later, the network worker's URL/job
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

  // Absolute qindaqt-file-manager candidates in trial order: the running
  // application's sibling binary (the shell and File Manager install into one
  // bindir), then the PATH lookup result. Relative names are never returned.
  [[nodiscard]] static QStringList fileManagerProgramCandidates();

  // Opens one listed local folder in QindaQt File Manager. The listed path must
  // still name the same object (`listed` device and inode) and must resolve
  // once to a readable, enterable canonical directory. Only then is the first
  // absolute executable candidate started detached with exactly that canonical
  // directory as its single argument; the default `start` is
  // QProcess::startDetached. No shell, URL handler, or default inode/directory
  // association is involved, and a refusal never falls back to another folder
  // or program. Success means the process started: File Manager revalidates
  // its folder argument and its later exit is not observed, the same boundary
  // launchLocalFile documents. GUI-thread only.
  [[nodiscard]] static FolderOpenResult openLocalFolder(
      const QString &absolutePath, ListedIdentity listed,
      const QStringList &programCandidates = fileManagerProgramCandidates(),
      const ProcessStarter &start = {});

  // AGENT-CONTRACT (ADR-0273): the one command line that starts File Manager
  // showing request.folder with request.names selected, then running
  // request.action there: one "--select=<name>" element per name, then
  // "--action=<id>" when there is an action, then the folder as the single
  // positional argument. File Manager's runtime/finder_integration reads and
  // runtime/process_reveal_windows writes exactly this shape. Every element
  // is one literal argv entry; nothing is joined into a command line.
  [[nodiscard]] static QStringList revealArguments(const RevealRequest &request);

  // Opens the folder holding one listed local item in QindaQt File Manager
  // with that item selected, then runs `action` on it there: empty to only
  // select it, "file.properties" for the Desktop's Get Info,
  // "file.open-with" for its Open With. The item must still be the object
  // `listed` names (the openLocalFolder identity rule) and its folder must
  // resolve once to a readable, enterable directory. Any other action is
  // UnsupportedAction. The program and launch rules, refusals and success
  // meaning are openLocalFolder's; on success `canonicalPath` is that folder.
  // GUI-thread only.
  [[nodiscard]] static FolderOpenResult revealLocalItem(
      const QString &absolutePath, ListedIdentity listed, const QString &action,
      const QStringList &programCandidates = fileManagerProgramCandidates(),
      const ProcessStarter &start = {});

  // Opens a folder the caller owns rather than listed (the Desktop's own
  // directory) in QindaQt File Manager with nothing selected, then runs one
  // isRevealAction() `action` there: "file.new-file" for the Desktop's New
  // File, "file.properties" for its Get Info. The folder must resolve once to
  // a readable, enterable directory; there is no listed identity to fence.
  // The program, launch, refusal and success rules are openLocalFolder's.
  // GUI-thread only.
  [[nodiscard]] static FolderOpenResult runLocalFolderAction(
      const QString &absolutePath, const QString &action,
      const QStringList &programCandidates = fileManagerProgramCandidates(),
      const ProcessStarter &start = {});

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

  // Composes one ClipboardController over an existing local mutation
  // controller and the platform clipboard, so Desktop-initiated
  // cut/copy/paste adopts exactly File Manager's own clipboard policy
  // (identity-checked own snapshots, foreign copy-only adoption, cut cleared
  // after commit). The caller owns the returned controller's lifetime; the
  // mutation controller must outlive it and must be the
  // createLocalMutationController composition above so paste dispatches into
  // the same identity-checked local authority.
  [[nodiscard]] static std::unique_ptr<ClipboardController>
  createLocalClipboardController(MutationController &mutation, QClipboard &clipboard,
                                 QObject *parent = nullptr);
};

} // namespace QindaQt::Apps::FileManager::Desktop
