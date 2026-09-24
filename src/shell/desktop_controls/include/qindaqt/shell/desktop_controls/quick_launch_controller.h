// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

namespace QindaQt::Services::DockItems {
class DockItems;
enum class DockEditError;
}

namespace QindaQt::Shell::Launcher {
class LauncherAppletController;
}

namespace QindaQt::ShellTaskListApplet {
class TaskListAppletController;
}

namespace QindaQt::Shell::DesktopControls {

class DockPathPort;

// The dock (Windows classic "quick launch", XFCE launchers, the macOS-style
// dock). Its rows ARE the launcher's dock value (ADR-0265): pinned
// applications, folders, files, named groups of applications, and Trash. This
// controller reads that value and the launcher's query-independent catalog
// values, and re-enters the same launcher facade for every edit, so there is
// exactly one pinned-application authority and one execution path (ADR-0062).
// Folders, files, and Trash open only through the injected DockPathPort (the
// File Manager's public boundary and the launcher's bounded process seam);
// running windows are only read from, and activated through, the task list
// facade.
//
// AGENT-CONTRACT: every borrowed collaborator may be null and must otherwise
// outlive this controller on the GUI thread. Indices in the invocables are
// dock-value indices (the `index` of a row), never Repeater positions; a
// `gap` is an insertion point in [0, itemCount] (see DockItems).
class QuickLaunchController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList rows READ rows NOTIFY stateChanged)
  Q_PROPERTY(QVariantList applicationRows READ applicationRows NOTIFY stateChanged)
  Q_PROPERTY(int count READ count NOTIFY stateChanged)
  Q_PROPERTY(int itemCount READ itemCount NOTIFY stateChanged)
  Q_PROPERTY(bool available READ available NOTIFY stateChanged)
  Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateChanged)
  Q_PROPERTY(bool launchGranted READ launchGranted CONSTANT)
  Q_PROPERTY(bool editable READ editable NOTIFY stateChanged)
  Q_PROPERTY(bool trashInDock READ trashInDock NOTIFY stateChanged)
  Q_PROPERTY(QStringList claimedTaskIds READ claimedTaskIds NOTIFY stateChanged)
  Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
  Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)

public:
  // Maps a window's application id to the desktop-entry id it belongs to
  // (the shell icon resolver's rules: exact id, StartupWMClass, reverse-DNS
  // tail). An empty answer means no entry claims the window.
  using DesktopEntryResolver = std::function<QString(const QString &applicationId)>;

  // Least-authority window grants from the quick-launch manifest. Read shows
  // running indicators and lets a dock host hide the task tiles the dock
  // already represents; activate brings a running pinned application forward
  // instead of starting it again.
  struct WindowGrants {
    bool windowsRead = false;
    bool windowsActivate = false;
  };

  QuickLaunchController(Launcher::LauncherAppletController *launcher,
                        bool applicationsLaunchGranted, QObject *parent = nullptr);

  // Optional collaborators, set once by the composition before QML binds.
  void setPathPort(DockPathPort *paths);
  void setWindowSource(ShellTaskListApplet::TaskListAppletController *taskList,
                       DesktopEntryResolver resolver, WindowGrants grants);

  // Rows, one per presented top-level item in dock order: {index, kind
  // ("application" | "folder" | "file" | "group" | "trash"), entryId, path,
  // displayText, iconName, accessibleName, accessibleDescription,
  // categoryIdentity, running, members, previewIcons}. Group
  // `members` are application rows of the same shape; `previewIcons` are the
  // first four member icons. Applications the catalog does not publish stay
  // stored but are not presented (the launcher's rule for stale pins).
  [[nodiscard]] QVariantList rows() const { return m_rows; }
  // Every presented application, group members in place of their group, in
  // the historical pinned-row shape {entryId, displayText, iconName,
  // accessibleName, accessibleDescription, index}; for application-only
  // presentations (application tiles).
  [[nodiscard]] QVariantList applicationRows() const { return m_applicationRows; }
  [[nodiscard]] int count() const noexcept
  {
    return static_cast<int>(m_rows.size());
  }
  // Stored top-level items, presented or not: the append gap.
  [[nodiscard]] int itemCount() const;
  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] QString phaseText() const;
  [[nodiscard]] bool launchGranted() const noexcept { return m_launchGranted; }
  [[nodiscard]] bool editable() const;
  [[nodiscard]] bool trashInDock() const noexcept { return m_trashInDock; }
  // Task ids of windows a top-level pinned application tile represents. A
  // dock host hides these task tiles ("one icon per app"); group members
  // keep their task tiles.
  [[nodiscard]] QStringList claimedTaskIds() const { return m_claimedTaskIds; }
  [[nodiscard]] bool feedbackPresent() const noexcept { return !m_feedback.isEmpty(); }
  [[nodiscard]] QString feedback() const { return m_feedback; }

  // Brings the application's window forward when it is running (and window
  // activation is granted), else starts it through the launcher.
  Q_INVOKABLE bool activate(const QString &entryId);
  // Starts another instance even when the application is running.
  Q_INVOKABLE bool openNewWindow(const QString &entryId);
  // Application, file, or Trash at index: activate or open it. A folder
  // opens in the File Manager (the stack popup is the QML's job).
  Q_INVOKABLE bool activateItem(int index);
  Q_INVOKABLE bool openInFileManager(int index);
  // A bounded listing of the pinned folder at index for its stack popup:
  // rows {name, path, isDirectory, iconName, device, inode}.
  Q_INVOKABLE QVariantList folderEntries(int index);
  // Opens one row folderEntries returned.
  Q_INVOKABLE bool openFolderEntry(const QVariantMap &entry);
  Q_INVOKABLE bool emptyTrash();

  Q_INVOKABLE bool unpin(const QString &entryId);
  Q_INVOKABLE bool moveUp(const QString &entryId);
  Q_INVOKABLE bool moveDown(const QString &entryId);
  Q_INVOKABLE bool moveItem(int index, int gap);
  Q_INVOKABLE bool removeItem(int index);
  // Drops from outside the dock: an application (by desktop-entry id) or
  // file/folder URLs. A `.desktop` file that names an installed application
  // becomes that application; the Trash files folder becomes the Trash item;
  // anything that is not a local file or folder is ignored.
  Q_INVOKABLE bool insertApplication(int gap, const QString &entryId);
  Q_INVOKABLE bool insertUrls(int gap, const QVariantList &urls);
  // Appends local paths as one edit (desktop icons "Add to Dock").
  Q_INVOKABLE bool addPaths(const QStringList &absolutePaths);
  // Groups: drop the application at sourceIndex onto the item at
  // targetIndex, or an application from outside the dock onto it.
  Q_INVOKABLE bool combineItems(int targetIndex, int sourceIndex, const QString &groupName);
  Q_INVOKABLE bool combineWithApplication(int targetIndex, const QString &entryId,
                                          const QString &groupName);
  Q_INVOKABLE bool newGroup(int index, const QString &groupName);
  Q_INVOKABLE bool renameGroup(int index, const QString &groupName);
  Q_INVOKABLE bool ungroup(int index);
  Q_INVOKABLE bool moveOutOfGroup(int groupIndex, const QString &entryId, int gap);
  Q_INVOKABLE bool showTrash();
  // The launcher category identity ("office", ...) for default group names.
  Q_INVOKABLE QString applicationCategory(const QString &entryId) const;
  // "Keep in Dock" for a running window, by its window application id.
  Q_INVOKABLE bool canKeepInDock(const QString &windowApplicationId) const;
  Q_INVOKABLE bool keepInDock(const QString &windowApplicationId);
  Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
  void stateChanged();
  void feedbackChanged();

private:
  struct RunningWindow {
    QString taskId;
    quint64 revision = 0;
    bool active = false;
  };

  void rebuild();
  void publishFeedback(const QString &message);
  [[nodiscard]] bool knownEntry(const QString &entryId) const;
  [[nodiscard]] bool launch(const QString &entryId);
  [[nodiscard]] QVariantMap applicationRow(const QString &entryId, int index,
                                           const QVariantMap &presentation) const;
  [[nodiscard]] QString desktopEntryForWindow(const QString &windowApplicationId) const;
  [[nodiscard]] QString applicationForDesktopFile(const QString &path) const;
  [[nodiscard]] const Services::DockItems::DockItems *dock() const;
  bool edit(const std::function<Services::DockItems::DockEditError(
                Services::DockItems::DockItems &)> &change);

  Launcher::LauncherAppletController *m_launcher = nullptr;
  ShellTaskListApplet::TaskListAppletController *m_taskList = nullptr;
  DockPathPort *m_paths = nullptr;
  DesktopEntryResolver m_resolver;
  WindowGrants m_windowGrants;
  bool m_launchGranted = false;
  QVariantList m_rows;
  QVariantList m_applicationRows;
  QStringList m_claimedTaskIds;
  QHash<QString, QList<RunningWindow>> m_running;
  bool m_trashInDock = false;
  // What the last stateChanged published besides the rows; -1 forces the
  // first rebuild to publish.
  bool m_publishedEditable = false;
  int m_publishedItemCount = -1;
  QString m_publishedPhase;
  QString m_feedback;
};

} // namespace QindaQt::Shell::DesktopControls
