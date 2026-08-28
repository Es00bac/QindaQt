// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "directory_lister.h"
#include "file_manager_types.h"
#include "launch_intent.h"
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

  [[nodiscard]] QString currentPath() const;
  [[nodiscard]] bool canGoBack() const;
  [[nodiscard]] bool canGoForward() const;
  [[nodiscard]] bool canGoUp() const;
  [[nodiscard]] QVariantList breadcrumb() const;
  [[nodiscard]] QString statusKey() const;
  [[nodiscard]] QString statusMessage() const;
  [[nodiscard]] QVariantList entries() const;
  [[nodiscard]] QString launchError() const;

  // Test seams independent of QML's QVariantList marshalling.
  [[nodiscard]] int entryCount() const;
  [[nodiscard]] const DirectoryEntry *entryAt(int index) const;
  [[nodiscard]] NavigationStatus status() const;

signals:
  void navigationChanged();
  void entriesChanged();
  void launchErrorChanged();

private:
  void reload();
  [[nodiscard]] static QString statusKeyFor(NavigationStatus status);

  DirectoryListerPtr m_lister;
  FileLauncherPtr m_launcher;
  NavigationHistory m_history;
  NavigationStatus m_status = NavigationStatus::Empty;
  QString m_statusMessage;
  QVector<DirectoryEntry> m_entries;
  QString m_launchError;
};

} // namespace QindaQt::Apps::FileManager
