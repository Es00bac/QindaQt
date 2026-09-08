// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app_shell/editor_app_shell_bridge.h"
#include "app_shell/file_selection_adapter.h"
#include "document/document_collection.h"
#include "editor_appearance.h"
#include "document_dialogs.h"
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
class QVBoxLayout;

namespace QindaQt::Apps::TextEditor {

class EditorDocumentView;
class FindReplaceBar;
class EditorApplication;
class TextEditorRestorePolicy;

// GUI-thread presentation owns one document, its view, dialogs and actions.
// The optional application owner and restore policy are borrowed and must
// outlive the window. New/Open cross the application window-inventory boundary.
class EditorWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit EditorWindow(
      DocumentStoreFactory storeFactory, EditorAppearance appearance,
      std::unique_ptr<FileSelectionAdapter> fileSelectionAdapter = nullptr,
      TextEditorRestorePolicy *restorePolicy = nullptr,
      EditorApplication *application = nullptr,
      std::unique_ptr<DocumentDialogs> dialogs = nullptr, QWidget *parent = nullptr);

  [[nodiscard]] DocumentController *controller() const;
  [[nodiscard]] QPlainTextEdit *editor() const;
  [[nodiscard]] FindReplaceBar *findBar() const { return m_findBar; }
  [[nodiscard]] QindaQt::AppShell::ApplicationCoordinator &
  appShellCoordinator();

  [[nodiscard]] bool openDocuments(const QStringList &paths,
                                   QString *diagnostic = nullptr);
  void applyAppearance(const EditorAppearance &appearance);
  void announceStatus(const QString &message);
  // Retained export is destroyed before the coordinator it observes.
  void setMenuExport(std::unique_ptr<QObject> menuExport);

signals:
  void firstFramePainted();
  void closeAccepted();
  void documentInventoryChanged();

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
  void createToolbar();
  void applyChrome();
  void createEditingActions();
  void createViewMenu();
  void publishAppShellProjection();
  void connectDocument();
  void connectRestorePolicy();
  void createDocumentView();
  void updateDocumentPresentation(DocumentController *controller = nullptr);
  void updateActionStates();

  [[nodiscard]] PendingAction confirmDocumentClose();
  [[nodiscard]] PendingAction confirmWindowClose();
  [[nodiscard]] bool saveDocument();
  [[nodiscard]] bool saveDocumentAs();
  void openInteractively();
  void newInteractively();
  void reloadInteractively();
  void showOperationError(const QString &title,
                          const DocumentOperation &result);

  void openFind(bool replaceMode);
  void findMatch(FindDirection direction);
  void replaceCurrent();
  void replaceAll();
  void selectFindResult(const FindResult &result);

  [[nodiscard]] bool addPath(const QString &path, QString *diagnostic);
  void persistRestoreState();

  struct Actions final {
    QAction *fileNew = nullptr;
    QAction *fileOpen = nullptr;
    QAction *fileCloseWindow = nullptr;
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
    QAction *restoreDocuments = nullptr;
    QList<QAction *> editingTools;
    QList<QAction *> viewTools;
  };

  DocumentController *m_document = nullptr;
  EditorAppearance m_appearance;
  Actions m_actions;
  QHash<QString, QAction *> m_appShellActionIds;
  EditorDocumentView *m_view = nullptr;
  EditorAppShellBridge m_appShellBridge;
  TextEditorRestorePolicy *m_restorePolicy = nullptr;
  EditorApplication *m_application = nullptr;
  QVBoxLayout *m_surfaceLayout = nullptr;
  FindReplaceBar *m_findBar = nullptr;
  bool m_firstFramePublished = false;
  bool m_pendingCloseApproved = false;
  std::unique_ptr<DocumentDialogs> m_dialogs;
  std::unique_ptr<QObject> m_menuExport;

};

} // namespace QindaQt::Apps::TextEditor
