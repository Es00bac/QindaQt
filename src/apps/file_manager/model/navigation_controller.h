// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "directory_lister.h"
#include "file_manager_types.h"
#include "launch_intent.h"
#include "listing_order.h"
#include "navigation_history.h"
#include "../network/network_directory_backend.h"
#include "../network/remote_file_opener.h"

#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVector>

namespace QindaQt::Apps::FileManager {

enum class NavigationStatus {
  Ready,
  Empty,
  PermissionDenied,
  Missing,
  NotADirectory,
  Error,
  // S5 network-browsing states (see NetworkListingError): an in-flight
  // asynchronous remote listing, and the remote-only typed failures a local
  // listing can never produce.
  Loading,
  Unavailable,
  AuthenticationRequired,
  Transport,
};

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

public:
  // networkBackend may be null: navigateTo() then refuses every smb/sftp
  // location with a typed Unavailable status instead of routing anywhere,
  // and every existing local-only caller/test is unaffected. remoteOpener
  // may also be null: remote regular-file activation then keeps reporting
  // the truthful "not supported yet" launchError instead of opening.
  NavigationController(DirectoryListerPtr lister, FileLauncherPtr launcher,
                       NetworkDirectoryBackendPtr networkBackend = nullptr,
                       RemoteFileOpenerPtr remoteOpener = nullptr,
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
  // Accepted values are "list" and "grid"; anything else is ignored.
  Q_INVOKABLE void setViewMode(const QString &mode);
  // GUI-thread, session-local literal filename matching; no I/O or recursion.
  // Truncates to the bound without splitting UTF-16 pairs. Actual directory
  // navigation clears the filter; refresh and presentation changes retain it.
  static constexpr int maximumNameFilterLength = 256;
  Q_INVOKABLE void setNameFilter(const QString &filter);
  // Steps through 32, 48, 64, 96 and 128 logical pixels with saturating bounds.
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
  [[nodiscard]] QString nameFilter() const;
  [[nodiscard]] bool guestListingActive() const { return m_guestActive; }
  [[nodiscard]] int nameFilterLengthLimit() const { return maximumNameFilterLength; }
  [[nodiscard]] int iconSize() const;
  [[nodiscard]] bool canZoomIn() const;
  [[nodiscard]] bool canZoomOut() const;
  [[nodiscard]] bool remoteActive() const noexcept { return m_remoteActive; }
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

private:
  void reload(bool resetFilter = false);
  // Re-derives the visible listing from m_listedEntries under the active
  // filter/order and republishes statusMessage. Callers emit entriesChanged
  // (and presentationChanged for user-facing setting changes) afterwards.
  void rebuildVisibleEntries();
  [[nodiscard]] static QString statusKeyFor(NavigationStatus status);
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
  [[nodiscard]] static NavigationStatus statusForNetworkError(NetworkListingError error);

  DirectoryListerPtr m_lister;
  FileLauncherPtr m_launcher;
  NetworkDirectoryBackendPtr m_networkBackend;
  RemoteFileOpenerPtr m_remoteOpener;
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
  QString m_guestStatusText;
  int m_iconSizeIndex = 2;
};

} // namespace QindaQt::Apps::FileManager
