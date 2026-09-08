// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "directory_lister.h"
#include "file_manager_types.h"
#include "launch_intent.h"
#include "listing_order.h"
#include "navigation_history.h"

#include <QObject>
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

public:
  NavigationController(DirectoryListerPtr lister, FileLauncherPtr launcher,
                       QObject *parent = nullptr);

  // Navigates as if the user chose path directly (breadcrumb segment, typed
  // path, or the very first navigation of the window's lifetime).
  Q_INVOKABLE void navigateTo(const QString &path);
  Q_INVOKABLE void goBack();
  Q_INVOKABLE void goForward();
  Q_INVOKABLE void goUp();
  Q_INVOKABLE void refresh();
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
  void reload();
  // Re-derives the visible listing from m_listedEntries under the active
  // filter/order and republishes statusMessage. Callers emit entriesChanged
  // (and presentationChanged for user-facing setting changes) afterwards.
  void rebuildVisibleEntries();
  [[nodiscard]] static QString statusKeyFor(NavigationStatus status);

  DirectoryListerPtr m_lister;
  FileLauncherPtr m_launcher;
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
};

} // namespace QindaQt::Apps::FileManager
