// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <qqmlintegration.h>

#include <functional>
#include <memory>
#include <optional>

class QClipboard;

namespace QindaQt::Apps::FileManager {
class ClipboardController;
class MutationController;
}

namespace QindaQt::Shell::DesktopSurface {

// Presents the user's Desktop-directory contents as desktop-icon rows
// (ADR-0125), replacing the earlier static Places projection: each row is a
// real file or folder read from the Desktop directory instead of the fixed
// XDG-places inventory.
//
// AGENT-CONTRACT: crosses into File Manager exclusively through
// `Apps::FileManager::Desktop::FileBoundary::listLocalFolder`,
// `launchLocalFile`, `openLocalFolder`, `revealLocalItem`,
// `runLocalFolderAction`, and `createLocalMutationController` (see
// module-boundaries.md); it never includes File
// Manager's model/**, mutation/**, or app_shell/** headers directly. A
// missing/unreadable root publishes zero rows plus `feedback` instead of
// leaving the surface input-blocked; hidden (dot) entries are omitted from
// the desktop presentation the same way every other stock desktop hides
// them. Like a File Manager view it follows its folder: a change another
// program makes there (File Manager's New File, a download) is re-listed
// shortly after (ADR-0273).
//
// Not final: QML_ELEMENT instantiates the type through a QQmlElement
// subclass.
class DesktopContentsController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  // Rows: {id, label, path, iconName, accessibleName, isDirectory}, one per
  // visible (non-hidden) Desktop-directory child, directories first.
  Q_PROPERTY(QVariantList rows READ rows NOTIFY rowsChanged)
  // Last listing/launch diagnostic; empty when the last operation succeeded.
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)
  // True when the composed clipboard holds pasteable content (File Manager's
  // own snapshot or an adopted foreign uri-list).
  Q_PROPERTY(bool canPaste READ canPaste NOTIFY clipboardChanged)
  // "copy", "cut", or "none" — the composed clipboard's current mode.
  Q_PROPERTY(QString clipboardMode READ clipboardMode NOTIFY clipboardChanged)

public:
  explicit DesktopContentsController(QObject *parent = nullptr);
  // Test seam: lists `root` instead of QStandardPaths::DesktopLocation, so
  // tests exercise a real temporary directory without touching the user's
  // home.
  explicit DesktopContentsController(QString root, QObject *parent = nullptr);
  // Test seam: also replaces FileBoundary::fileManagerProgramCandidates() so a
  // folder activation starts a recording stand-in instead of File Manager.
  DesktopContentsController(QString root, QStringList fileManagerPrograms,
                            QObject *parent = nullptr);
  // Test seam: also injects the clipboard the cut/copy/paste policy composes
  // over. Passing nullptr (the production default below) leaves clipboard
  // operations disabled/fail-closed, which keeps GUI-less controller tests
  // able to exercise everything else.
  DesktopContentsController(QString root, std::optional<QStringList> fileManagerPrograms,
                            QClipboard *clipboard, QObject *parent = nullptr);
  ~DesktopContentsController() override;

  [[nodiscard]] QVariantList rows() const { return m_rows; }
  [[nodiscard]] QString feedback() const { return m_feedback; }
  // True when the composed clipboard holds pasteable content (File Manager's
  // own snapshot or an adopted foreign uri-list).
  [[nodiscard]] bool canPaste() const;
  // "copy", "cut", or "none" — the composed clipboard's current mode.
  [[nodiscard]] QString clipboardMode() const;

  // Re-lists the root directory through the boundary. The desktop context
  // menu's Arrange/Refresh/Clean Up/New Folder entries call this so the icon
  // set reflects the directory's current bounded-local-I/O state.
  Q_INVOKABLE void refresh();
  // Activates one entry from the last listing. A listed directory opens in
  // QindaQt File Manager through FileBoundary::openLocalFolder, fenced by the
  // identity the listing reported; a listed regular file keeps the bounded
  // default-handler launch through FileBoundary::launchLocalFile. A path the
  // listing never reported, or any typed boundary refusal, publishes
  // `feedback`, returns false, and launches nothing.
  Q_INVOKABLE bool open(const QString &absolutePath);
  // Hands one File Manager dialog to File Manager (ADR-0273): Get Info
  // ("file.properties") or Open With ("file.open-with") for one entry of the
  // last listing, fenced by the identity the listing reported, or, with no
  // path, New File ("file.new-file") or Get Info in the Desktop folder
  // itself. File Manager opens on the Desktop folder, with the entry selected,
  // and runs the action there. An unlisted path, another action, or any typed
  // boundary refusal publishes `feedback`, returns false, and starts nothing.
  Q_INVOKABLE bool runFileManagerAction(const QString &actionId,
                                        const QString &absolutePath = {});
  // Renames only an entry from the last listing, using the complete listing-
  // time identity consumed by File Manager's asynchronous mutation boundary.
  Q_INVOKABLE bool rename(const QString &absolutePath, const QString &newName);
  // Batch-moves the given entries to the home Trash. Each item must be one of
  // this controller's own row maps (or carry at least its "path" plus the
  // identity fields); anything the last listing did not report is refused
  // with `feedback` before any item is trashed.
  Q_INVOKABLE bool trashEntries(const QVariantList &items);
  // Copies/cuts the given entries to the composed clipboard; paste lands in
  // the Desktop directory through the same identity-checked batch contract
  // File Manager's own clipboard uses.
  Q_INVOKABLE bool copySelection(const QVariantList &items);
  Q_INVOKABLE bool cutSelection(const QVariantList &items);
  Q_INVOKABLE bool pasteIntoDesktop();
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void rowsChanged();
  void feedbackChanged();
  void clipboardChanged();

private:
  // Shared validation: rebuilds the mutation batch item maps (path plus
  // listing-time identity) for exactly the entries the last listing
  // reported; returns false (with `diagnostic` set) otherwise.
  [[nodiscard]] bool collectBatchItems(const QVariantList &items,
                                       QVariantList *batch,
                                       QString *diagnostic) const;
  // Shared copy/cut path: validates the selection into batch item maps, then
  // dispatches through the composed clipboard policy.
  [[nodiscard]] bool dispatchClipboardSelection(
      const QVariantList &items,
      const std::function<bool(const QVariantList &)> &dispatch);

private:
  struct ListedEntry {
    bool isDirectory = false;
    quint64 device = 0;
    quint64 inode = 0;
    qint64 identitySize = 0;
    qint64 modifiedNanoseconds = 0;
    quint32 mode = 0;
  };

  void initializeMutation();
  void publishFeedback(const QString &message);

  QString m_root;
  // ADR-0273: re-lists the root shortly after it changes on disk; a burst of
  // changes (a copy of many files) becomes one refresh.
  QFileSystemWatcher m_watcher;
  QTimer m_refreshTimer;
  // Unset means FileBoundary's production program candidates.
  std::optional<QStringList> m_fileManagerPrograms;
  QClipboard *m_clipboard = nullptr;
  QHash<QString, ListedEntry> m_listed;
  QVariantList m_rows;
  QString m_feedback;
  std::unique_ptr<QindaQt::Apps::FileManager::MutationController> m_mutation;
  std::unique_ptr<QindaQt::Apps::FileManager::ClipboardController> m_clipboardController;
};

} // namespace QindaQt::Shell::DesktopSurface
