// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app_shell/editor_app_shell_bridge.h"
#include "app_shell/file_selection_adapter.h"
#include "document/document_collection.h"
#include "editor_appearance.h"
#include "find/find_replace_engine.h"

#include <QHash>
#include <QList>
#include <QMainWindow>

#include <memory>

class QAction;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QPaintEvent;
class QPlainTextEdit;
class QTabWidget;

namespace QindaQt::Apps::TextEditor {

class EditorDocumentView;
class FindReplaceBar;
class RestoreStateStore;
class TextEditorRestorePolicy;

// Presentation owns tabs, dialogs, consent, focus, and command routing. The
// injected collection factory creates one store/controller per document; the
// optional policy and state-store pointers are non-owning and must outlive the
// window. No filesystem work occurs here except through those collaborators.
class EditorWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit EditorWindow(
      DocumentStoreFactory storeFactory, EditorAppearance appearance,
      std::unique_ptr<FileSelectionAdapter> fileSelectionAdapter = nullptr,
      TextEditorRestorePolicy *restorePolicy = nullptr,
      RestoreStateStore *restoreStore = nullptr, QWidget *parent = nullptr);

  [[nodiscard]] DocumentController *controller() const;
  [[nodiscard]] QPlainTextEdit *editor() const;
  [[nodiscard]] DocumentCollection *documents() const { return m_documents; }
  [[nodiscard]] QTabWidget *tabs() const { return m_tabs; }
  [[nodiscard]] FindReplaceBar *findBar() const { return m_findBar; }
  [[nodiscard]] QindaQt::AppShell::ApplicationCoordinator &
  appShellCoordinator();

  [[nodiscard]] bool openDocuments(const QStringList &paths,
                                   QString *diagnostic = nullptr);
  void restoreIfEnabled();

signals:
  void firstFramePainted();

protected:
  void closeEvent(QCloseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  enum class PendingAction { Continue, Cancel };

  void createCentralSurface();
  void createActions();
  void createMenus();
  void publishAppShellProjection();
  void connectCollection();
  void connectRestorePolicy();
  void addDocumentView(DocumentController *controller, int index);
  void removeDocumentView(DocumentController *controller, int index);
  void updateDocumentPresentation(DocumentController *controller = nullptr);
  void updateActionStates();
  void selectRelativeTab(int delta);
  void selectTab(int index);

  [[nodiscard]] PendingAction confirmDocumentClose(int index);
  [[nodiscard]] PendingAction confirmWindowClose();
  [[nodiscard]] bool saveDocument(int index);
  [[nodiscard]] bool saveDocumentAs(int index);
  void closeTab(int index);
  void openInteractively();
  void newInteractively();
  void reloadInteractively(int index);
  void showOperationError(const QString &title,
                          const DocumentOperation &result);

  void openFind(bool replaceMode);
  void findMatch(FindDirection direction);
  void replaceCurrent();
  void replaceAll();
  void selectFindResult(const FindResult &result);

  [[nodiscard]] bool addPath(const QString &path, QString *diagnostic);
  void persistRestoreState();
  void announceStatus(const QString &message);

  struct Actions final {
    QAction *fileNew = nullptr;
    QAction *fileOpen = nullptr;
    QAction *fileCloseTab = nullptr;
    QAction *fileSave = nullptr;
    QAction *fileSaveAs = nullptr;
    QAction *fileQuit = nullptr;
    QAction *editUndo = nullptr;
    QAction *editRedo = nullptr;
    QAction *editCut = nullptr;
    QAction *editCopy = nullptr;
    QAction *editPaste = nullptr;
    QAction *editSelectAll = nullptr;
    QAction *editFind = nullptr;
    QAction *editReplace = nullptr;
    QAction *editFindNext = nullptr;
    QAction *editFindPrevious = nullptr;
    QAction *editFindClose = nullptr;
    QAction *tabNext = nullptr;
    QAction *tabPrevious = nullptr;
    QList<QAction *> tabSelect;
    QAction *restoreDocuments = nullptr;
  };

  DocumentCollection *m_documents = nullptr;
  EditorAppearance m_appearance;
  Actions m_actions;
  QHash<QString, QAction *> m_appShellActionIds;
  QHash<DocumentController *, EditorDocumentView *> m_views;
  EditorAppShellBridge m_appShellBridge;
  TextEditorRestorePolicy *m_restorePolicy = nullptr;
  RestoreStateStore *m_restoreStore = nullptr;
  QTabWidget *m_tabs = nullptr;
  FindReplaceBar *m_findBar = nullptr;
  bool m_firstFramePublished = false;
  bool m_pendingCloseApproved = false;
  bool m_explicitStartupPaths = false;
  bool m_restoreAttempted = false;
};

} // namespace QindaQt::Apps::TextEditor
