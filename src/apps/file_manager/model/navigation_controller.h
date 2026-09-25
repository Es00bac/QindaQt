// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "applications_location.h"
#include "directory_lister.h"
#include "file_manager_types.h"
#include "launch_intent.h"
#include "listing_order.h"
#include "navigation_history.h"
#include "navigation_status.h"
#include "../network/network_directory_backend.h"
#include "../network/remote_copy_to_controller.h"
#include "../network/remote_copier.h"
#include "../network/remote_create_folder_controller.h"
#include "../network/remote_file_opener.h"
#include "../network/remote_folder_creator.h"
#include "../network/remote_move_to_controller.h"
#include "../network/remote_mover.h"
#include "../network/remote_open_controller.h"
#include "../network/remote_rename_controller.h"
#include "../network/remote_renamer.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVector>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT: This GUI-thread QObject owns the injected lister/launcher
// and all navigation/listing state for one window. It never blocks longer
// than one synchronous local directory read, never shows a dialog, and
// publishes every failure as a typed status plus a human-readable message
// instead of throwing or leaving stale entries visible after a failed
// navigation. It never chooses which entry is selected; QML owns
// presentation-only selection/focus and may query indexOfName() to restore a
// deterministic selection across a refresh.
class NavigationController final : public QObject {
  Q_OBJECT

  Q_PROPERTY(QString currentPath READ currentPath NOTIFY navigationChanged FINAL)
  Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY navigationChanged FINAL)
  Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY navigationChanged FINAL)
  Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY navigationChanged FINAL)
  Q_PROPERTY(QVariantList breadcrumb READ breadcrumb NOTIFY navigationChanged FINAL)
  Q_PROPERTY(QString statusKey READ statusKey NOTIFY entriesChanged FINAL)
  Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY entriesChanged FINAL)
  Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged FINAL)
  Q_PROPERTY(QString launchError READ launchError NOTIFY launchErrorChanged FINAL)
  Q_PROPERTY(QString sortColumn READ sortColumn NOTIFY presentationChanged FINAL)
  Q_PROPERTY(QString sortDirection READ sortDirection NOTIFY presentationChanged FINAL)
  Q_PROPERTY(bool directoriesFirst READ directoriesFirst NOTIFY presentationChanged FINAL)
  Q_PROPERTY(bool showHidden READ showHidden NOTIFY presentationChanged FINAL)
  Q_PROPERTY(QString viewMode READ viewMode NOTIFY presentationChanged FINAL)
  // ADR-0270: Group By ("none", "kind", "date", "size"), part of the order.
  Q_PROPERTY(QString groupBy READ groupBy NOTIFY presentationChanged FINAL)
  Q_PROPERTY(bool folderViewActive READ folderViewActive WRITE setFolderViewActive
                 NOTIFY presentationChanged FINAL)
  Q_PROPERTY(QString nameFilter READ nameFilter NOTIFY presentationChanged FINAL)
  Q_PROPERTY(bool guestListingActive READ guestListingActive NOTIFY entriesChanged FINAL)
  Q_PROPERTY(int maximumNameFilterLength READ nameFilterLengthLimit CONSTANT FINAL)
  Q_PROPERTY(int iconSize READ iconSize NOTIFY presentationChanged FINAL)
  Q_PROPERTY(bool canZoomIn READ canZoomIn NOTIFY presentationChanged FINAL)
  Q_PROPERTY(bool canZoomOut READ canZoomOut NOTIFY presentationChanged FINAL)
  // True while currentPath() is a canonical smb/sftp URL. QML and the
  // AppShell action bindings disable every local-only action (mutation,
  // preview, recursive search, bounded local launch) while this is true;
  // navigation itself (back/forward/up/refresh/location entry) stays live.
  Q_PROPERTY(bool remoteActive READ remoteActive NOTIFY navigationChanged FINAL)
  // ADR-0262: true while the current location is the Applications place.
  Q_PROPERTY(bool applicationsPlace READ applicationsPlace NOTIFY navigationChanged FINAL)
  // ADR-0153: true while browsing remote with a RemoteRenamer injected --
  // the action bindings use this to keep "file.rename" available remotely.
  Q_PROPERTY(bool remoteRenameAvailable READ remoteRenameAvailable NOTIFY navigationChanged FINAL)
  // True while one remote rename is in flight; re-entrant requests are
  // refused and the rename action disables to prevent overlap.
  Q_PROPERTY(bool remoteRenameBusy READ remoteRenameBusy NOTIFY remoteRenameChanged FINAL)
  // ADR-0154: true while browsing remote with a RemoteFolderCreator
  // injected -- the action bindings use this to keep "file.new-folder"
  // available remotely.
  Q_PROPERTY(bool remoteCreateAvailable READ remoteCreateAvailable NOTIFY navigationChanged FINAL)
  // True while one remote folder creation is in flight.
  Q_PROPERTY(bool remoteCreateBusy READ remoteCreateBusy NOTIFY remoteCreateChanged FINAL)
  // ADR-0155: true while browsing remote with a RemoteCopier injected --
  // the action bindings use this to keep "file.copy" available remotely.
  Q_PROPERTY(bool remoteCopyAvailable READ remoteCopyAvailable NOTIFY navigationChanged FINAL)
  // True while one remote copy is in flight.
  Q_PROPERTY(bool remoteCopyBusy READ remoteCopyBusy NOTIFY remoteCopyChanged FINAL)
  // ADR-0156: true while browsing remote with a RemoteMover injected --
  // the action bindings use this to keep "file.move" available remotely.
  Q_PROPERTY(bool remoteMoveAvailable READ remoteMoveAvailable NOTIFY navigationChanged FINAL)
  // True while one remote move is in flight. A move is destructive, so the
  // move action disables while busy to prevent overlap.
  Q_PROPERTY(bool remoteMoveBusy READ remoteMoveBusy NOTIFY remoteMoveChanged FINAL)

public:
  // Window-local presentation only; never persisted. All folder action
  // bindings use this so hidden folder selections cannot receive shortcuts.
  [[nodiscard]] bool folderViewActive() const { return m_folderViewActive; }
  void setFolderViewActive(bool active) {
    if (m_folderViewActive == active) return;
    m_folderViewActive = active;
    emit presentationChanged();
  }

  // networkBackend may be null: navigateTo() then refuses every smb/sftp
  // location with a typed Unavailable status instead of routing anywhere,
  // and every existing local-only caller/test is unaffected. remoteOpener
  // may also be null: remote regular-file activation then keeps reporting
  // the truthful "not supported yet" launchError instead of opening.
  // remoteRenamer may be null: remote Rename then stays disabled by the
  // action bindings exactly as before this seam existed. folderCreator may
  // be null: remote New Folder then stays disabled the same way. copier
  // may be null: remote Copy To then stays disabled the same way. mover
  // may be null: remote Move To then stays disabled the same way.
  NavigationController(DirectoryListerPtr lister, FileLauncherPtr launcher,
                       NetworkDirectoryBackendPtr networkBackend = nullptr,
                       RemoteFileOpenerPtr remoteOpener = nullptr,
                       RemoteRenamerPtr remoteRenamer = nullptr,
                       RemoteFolderCreatorPtr folderCreator = nullptr,
                       RemoteCopierPtr copier = nullptr,
                       RemoteMoverPtr mover = nullptr,
                       QObject *parent = nullptr);

  // Navigates as if the user chose path directly (breadcrumb segment, typed
  // path, or the very first navigation of the window's lifetime).
  Q_INVOKABLE void navigateTo(const QString &path);
  Q_INVOKABLE void goBack();
  Q_INVOKABLE void goForward();
  Q_INVOKABLE void goUp();
  Q_INVOKABLE void refresh();
  // Publishes an externally produced result set (recursive search) as the
  // visible listing of the current folder. Guest entries flow through the
  // same hidden/name-filter/sort projection as listed entries; activate()
  // uses their absolute paths, so they open from anywhere in the tree.
  // refresh() and every navigation drop the guest listing and re-read the
  // real folder. The controller never produces guest entries itself.
  Q_INVOKABLE void showGuestListing(const QVector<DirectoryEntry> &entries,
                                    const QString &statusText);
  Q_INVOKABLE void clearGuestListing();
  // Opens the entry at index: navigates into a directory, or requests a
  // bounded launch for a file. Out-of-range indexes are ignored.
  Q_INVOKABLE void activate(int index);
  Q_INVOKABLE void clearLaunchError();
  // ADR-0153: renames one listed child of the current remote folder to a
  // validated sibling name through the injected RemoteRenamer, then
  // refreshes the authoritative listing on success. Rejects everything else
  // (unknown name, separators, dot names, unlisted or cross-folder sources,
  // overlapping renames) before dispatch and reports the failure through
  // launchError. Returns false when the request was refused before dispatch.
  Q_INVOKABLE bool renameRemoteEntry(const QString &sourcePath, const QString &newName);
  // ADR-0154: creates one validated child directory in the current remote
  // folder through the injected RemoteFolderCreator, then refreshes the
  // authoritative listing on confirmed success. Rejects invalid/traversal
  // names and overlapping creates before dispatch and reports failures
  // through launchError; no optimistic entry is ever displayed. Returns
  // false when the request was refused before dispatch.
  Q_INVOKABLE bool createRemoteFolder(const QString &name);
  // ADR-0155: copies one listed child of the current remote folder to a
  // validated remote destination folder through the injected RemoteCopier.
  // Rejects unlisted/cross-folder sources, malformed destinations,
  // same-target copies, and directory self/descendant copies before
  // dispatch; failures surface through launchError and the listing
  // refreshes only when the confirmed destination is the current folder.
  // Returns false when the request was refused before dispatch.
  Q_INVOKABLE bool copyRemoteChild(const QString &sourcePath, const QString &destinationFolder);
  // ADR-0155: user-facing cancellation backing the shared operation.cancel
  // action (Ctrl+Escape). Retires an in-flight remote copy exactly like
  // navigation replacement does -- the copier kills the KIO job quietly and
  // the generation fence discards its late result -- so Cancel works without
  // navigating away. No-op when no remote copy is active.
  Q_INVOKABLE void cancelRemoteCopy();
  // ADR-0156: moves one listed child of the current remote folder to a
  // validated remote destination folder through the injected RemoteMover.
  // A move is destructive (the server may delete the source even on a
  // mid-move failure), so unlisted/cross-folder sources, malformed
  // destinations, same-target moves, and directory self/descendant moves are
  // rejected before dispatch; failures surface through launchError and the
  // listing refreshes after every confirmed success, because the source
  // always left the folder being viewed. Returns false when the request was
  // refused before dispatch.
  Q_INVOKABLE bool moveRemoteChild(const QString &sourcePath, const QString &destinationFolder);
  // ADR-0156: user-facing cancellation backing the shared operation.cancel
  // action while a remote move is in flight. Retires it exactly like
  // navigation replacement does -- the mover kills the KIO job quietly and
  // the generation fence discards its late result. No-op when no remote move
  // is active. Also called internally when remote browsing is replaced.
  Q_INVOKABLE void cancelRemoteMove();
  // Returns the index of the entry named name in the current listing, or -1.
  // QML uses this to restore a deterministic selection across a refresh.
  Q_INVOKABLE int indexOfName(const QString &name) const;
  // Applies a new sort column; calling with the active column flips the
  // direction instead (standard header-click behavior). Unknown keys are
  // ignored. Changing presentation re-sorts the already-listed entries
  // without re-reading the directory.
  Q_INVOKABLE void setSortColumn(const QString &columnKey);
  Q_INVOKABLE void setShowHidden(bool showHidden);
  Q_INVOKABLE void setDirectoriesFirst(bool directoriesFirst);
  // Accepted values are "list" (Details), "grid" (Icons), "columns" and
  // "gallery" (ADR-0270); anything else is ignored.
  Q_INVOKABLE void setViewMode(const QString &mode);
  // Groups the listing by an entryGroupKey(); unknown keys are ignored. Like
  // sorting, it re-orders the already-listed entries without re-reading.
  Q_INVOKABLE void setGroupBy(const QString &groupKey);
  // GUI-thread, session-local literal filename matching; no I/O or recursion.
  // Truncates to the bound without splitting UTF-16 pairs. Actual directory
  // navigation clears the filter; refresh and presentation changes retain it.
  static constexpr int maximumNameFilterLength = 256;
  Q_INVOKABLE void setNameFilter(const QString &filter);
  // Steps through 16, 24, 32, 48, 64, 96 and 128 logical pixels with
  // saturating bounds.
  // Changes presentation only: listing identities and generations stay intact.
  Q_INVOKABLE void zoomBy(int steps);
  Q_INVOKABLE void resetZoom();

  [[nodiscard]] QString currentPath() const;
  [[nodiscard]] bool canGoBack() const;
  [[nodiscard]] bool canGoForward() const;
  [[nodiscard]] bool canGoUp() const;
  [[nodiscard]] QVariantList breadcrumb() const;
  [[nodiscard]] QString statusKey() const;
  [[nodiscard]] QString statusMessage() const;
  [[nodiscard]] QVariantList entries() const;
  [[nodiscard]] QString launchError() const;
  [[nodiscard]] QString sortColumn() const;
  [[nodiscard]] QString sortDirection() const;
  [[nodiscard]] bool directoriesFirst() const;
  [[nodiscard]] bool showHidden() const;
  [[nodiscard]] QString viewMode() const;
  [[nodiscard]] QString groupBy() const { return entryGroupKey(m_order.group); }
  [[nodiscard]] QString nameFilter() const;
  [[nodiscard]] bool guestListingActive() const { return m_guestActive; }
  [[nodiscard]] int nameFilterLengthLimit() const { return maximumNameFilterLength; }
  [[nodiscard]] int iconSize() const;
  [[nodiscard]] bool canZoomIn() const;
  [[nodiscard]] bool canZoomOut() const;
  [[nodiscard]] bool remoteActive() const noexcept { return m_remoteActive; }
  [[nodiscard]] bool applicationsPlace() const { return ApplicationsLocation::isLocation(currentPath()); }
  [[nodiscard]] bool remoteRenameAvailable() const noexcept {
    return m_remoteActive && m_remoteRename != nullptr;
  }
  [[nodiscard]] bool remoteRenameBusy() const noexcept {
    return m_remoteRename != nullptr && m_remoteRename->busy();
  }
  [[nodiscard]] bool remoteCreateAvailable() const noexcept {
    return m_remoteActive && m_folderCreator != nullptr;
  }
  [[nodiscard]] bool remoteCreateBusy() const noexcept {
    return m_remoteCreate != nullptr && m_remoteCreate->busy();
  }
  [[nodiscard]] bool remoteCopyAvailable() const noexcept {
    return m_remoteActive && m_copier != nullptr;
  }
  [[nodiscard]] bool remoteCopyBusy() const noexcept {
    return m_remoteCopy != nullptr && m_remoteCopy->busy();
  }
  [[nodiscard]] bool remoteMoveAvailable() const noexcept {
    return m_remoteActive && m_mover != nullptr;
  }
  [[nodiscard]] bool remoteMoveBusy() const noexcept {
    return m_remoteMove != nullptr && m_remoteMove->busy();
  }
  [[nodiscard]] quint64 listingGeneration() const { return m_listingGeneration; }

  // Test seams independent of QML's QVariantList marshalling. entryCount and
  // entryAt expose the visible (filtered and sorted) listing QML sees.
  [[nodiscard]] int entryCount() const;
  [[nodiscard]] const DirectoryEntry *entryAt(int index) const;
  [[nodiscard]] NavigationStatus status() const;

signals:
  void navigationChanged();
  void entriesChanged();
  void launchErrorChanged();
  void presentationChanged();
  void remoteRenameChanged();
  void remoteCreateChanged();
  void remoteCopyChanged();
  void remoteMoveChanged();

private:
  void reload(bool resetFilter = false);
  // Re-derives the visible listing from m_listedEntries under the active
  // filter/order and republishes statusMessage. Callers emit entriesChanged
  // (and presentationChanged for user-facing setting changes) afterwards.
  void rebuildVisibleEntries();
  // Marks a fresh remote navigation (drops any guest listing/name filter)
  // and starts its first listing request.
  void enterRemote(const QUrl &url);
  // Starts (or restarts) the async remote listing for m_remoteUrl at a
  // freshly bumped m_listingGeneration; sets NavigationStatus::Loading
  // immediately so the UI never shows stale entries while waiting.
  void requestRemoteListing();
  // Fenced NetworkDirectoryBackend::listingReady handler: a generation or
  // URL mismatch (superseded navigation, or a callback arriving after
  // clearing/leaving the remote location) is silently discarded.
  void onNetworkListingReady(quint64 generation, const QUrl &url,
                             const NetworkListingResult &result);
  // ADR-0152: hands a remote regular file to the injected opener, or reports
  // the truthful "not supported yet" error when none is injected.
  void activateRemoteFile(const DirectoryEntry &entry);
  // ADR-0153: fenced rename result wiring lives in RemoteRenameController;
  // these hooks just bridge it to navigation state.
  void cancelPendingRemoteRename();
  void cancelPendingRemoteCreate();
  // ADR-0155/0156: same bridging for RemoteCopyToController and
  // RemoteMoveToController; all remote operation failures share the
  // launchError surface and all refresh requests share the listing re-read.
  // (Copy and Move retire through their public Q_INVOKABLE hooks, which
  // internal leave/replace paths call directly.)
  void onRemoteOperationRefreshRequested();
  void onRemoteOperationFailed(const QString &message);

  DirectoryListerPtr m_lister;
  FileLauncherPtr m_launcher;
  NetworkDirectoryBackendPtr m_networkBackend;
  RemoteFileOpenerPtr m_remoteOpener;
  RemoteRenamerPtr m_remoteRenamer;
  RemoteFolderCreatorPtr m_folderCreator;
  RemoteCopierPtr m_copier;
  RemoteMoverPtr m_mover;
  std::unique_ptr<RemoteOpenController> m_remoteOpen;
  std::unique_ptr<RemoteRenameController> m_remoteRename;
  std::unique_ptr<RemoteCreateFolderController> m_remoteCreate;
  std::unique_ptr<RemoteCopyToController> m_remoteCopy;
  std::unique_ptr<RemoteMoveToController> m_remoteMove;
  bool m_remoteActive = false;
  QUrl m_remoteUrl;
  NavigationHistory m_history;
  NavigationStatus m_status = NavigationStatus::Empty;
  QString m_statusMessage;
  QVector<DirectoryEntry> m_listedEntries;
  QVector<DirectoryEntry> m_entries;
  QString m_launchError;
  ListingOrder m_order;
  bool m_showHidden = false;
  bool m_truncated = false;
  int m_hiddenFilteredCount = 0;
  quint64 m_listingGeneration = 0;
  QString m_viewMode = QStringLiteral("grid");
  QString m_nameFilter;
  bool m_guestActive = false;
  bool m_folderViewActive = true;
  QString m_guestStatusText;
  int m_iconSizeIndex = 4;
};

} // namespace QindaQt::Apps::FileManager
