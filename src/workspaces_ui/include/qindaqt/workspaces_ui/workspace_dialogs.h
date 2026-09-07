// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/workspaces/workspace_store.h>

#include <QDialog>

class QListWidget;
class QLabel;

namespace QindaQt::WorkspacesUi {

// The compositor adapter supplies these owned values. A title is presentation
// only; Workspaces assignment remains the authority for identity and eligibility.
struct CurrentContainer {
  Core::WindowContainer layout;
  QMap<QString, Workspaces::ApplicationSlot> applicationsByWindow;
  QString name;
  QString color;
  // Empty creates a new saved workspace; a supplied ID replaces that document.
  QString workspaceId;
};

struct WorkspaceWindow {
  Workspaces::AvailableWindow window;
  QString title;
};

struct InstalledApplication {
  QString desktopEntryId;
  QString displayName;
};

// AGENT-CONTRACT: The UI must never retain platform objects or infer matching.
// Calls are synchronous on the GUI thread, and the borrowed port outlives its
// dialog. The compositor adapter returns snapshots and revalidates `restore()`
// atomically; a launch reports dispatch only and never waits for an application.
class WorkspaceUiPort {
public:
  virtual ~WorkspaceUiPort() = default;
  [[nodiscard]] virtual std::optional<CurrentContainer>
  currentContainer(QString *error) = 0;
  [[nodiscard]] virtual QList<WorkspaceWindow>
  availableWindows(QString *error) = 0;
  [[nodiscard]] virtual std::optional<InstalledApplication>
  installedApplication(const QString &desktopEntryId) const = 0;
  virtual bool launchApplication(const QString &desktopEntryId,
                                 const QStringList &urls,
                                 QString *error) = 0;
  virtual bool restore(const Workspaces::Workspace &workspace,
                       const Core::WindowContainer &boundLayout,
                       QString *error) = 0;
};

// A compact native entry point for a caller-owned durable workspace directory.
// It is GUI-thread confined; callers serialize the synchronous store separately.
class WorkspaceLibraryDialog final : public QDialog {
  Q_OBJECT
public:
  WorkspaceLibraryDialog(QString storageRoot, WorkspaceUiPort &port,
                         QWidget *parent = nullptr);
  void reload();

private slots:
  void saveCurrent();
  void reopenSelected();

public slots:
  // Queued platform launch completion calls this on the GUI thread. It keeps an
  // open assignment dialog and its explicit choices intact.
  void reportLaunchFailure(QString desktopEntryId, QString message);

signals:
  void launchFailureReported(QString desktopEntryId, QString message);

private:
  Workspaces::WorkspaceStore m_store;
  WorkspaceUiPort &m_port;
  QListWidget *m_list = nullptr;
  QLabel *m_result = nullptr;
};

} // namespace QindaQt::WorkspacesUi
