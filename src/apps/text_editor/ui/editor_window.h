// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "app_shell/editor_app_shell_bridge.h"
#include "app_shell/file_selection_adapter.h"
#include "document/document_controller.h"
#include "editor_appearance.h"

#include <QHash>
#include <QMainWindow>

#include <memory>

class QAction;
class QLabel;
class QPaintEvent;
class QPlainTextEdit;
class QPushButton;
class QWidget;

namespace QindaQt::Apps::TextEditor {

// Presentation owns dialogs and consent. It consumes the controller's values
// and standard QAction tree but never performs filesystem operations directly.
// The window takes exclusive ownership of a non-null store and all child
// widgets/controllers live on the constructing GUI thread until window
// teardown. It also owns the one EditorAppShellBridge that publishes this
// window's action/menu facts and mediates close/file-selection requests
// through QindaQt.AppShell 1.0; the window retains all consent and command
// authority, per docs/wiki/apps/application-shell.md.
class EditorWindow final : public QMainWindow {
  Q_OBJECT

public:
  explicit EditorWindow(
      DocumentStorePtr store, EditorAppearance appearance,
      std::unique_ptr<FileSelectionAdapter> fileSelectionAdapter = nullptr,
      QWidget *parent = nullptr);

  [[nodiscard]] DocumentController *controller() const;
  [[nodiscard]] QPlainTextEdit *editor() const;
  [[nodiscard]] QindaQt::AppShell::ApplicationCoordinator &
  appShellCoordinator();

signals:
  void firstFramePainted();

protected:
  void closeEvent(QCloseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  enum class PendingAction { Continue, Cancel };

  void createCentralSurface();
  void createActions();
  void createMenus();
  void publishAppShellProjection();
  void connectState();
  void applyAppearance();
  void applyExternalBannerAppearance(ExternalState state);
  void updateDocumentPresentation();
  void updateExternalBanner(ExternalState state);
  void announceExternalState(ExternalState state);
  [[nodiscard]] PendingAction confirmDiscardOrSave();
  [[nodiscard]] bool saveInteractively();
  [[nodiscard]] bool saveAsInteractively();
  void openInteractively();
  void newInteractively();
  void reloadInteractively();
  void showOperationError(const QString &title,
                          const DocumentOperation &result);

  struct Actions final {
    QAction *fileNew = nullptr;
    QAction *fileOpen = nullptr;
    QAction *fileSave = nullptr;
    QAction *fileSaveAs = nullptr;
    QAction *fileQuit = nullptr;
    QAction *editUndo = nullptr;
    QAction *editRedo = nullptr;
    QAction *editCut = nullptr;
    QAction *editCopy = nullptr;
    QAction *editPaste = nullptr;
    QAction *editSelectAll = nullptr;
  };

  DocumentController *m_controller = nullptr;
  EditorAppearance m_appearance;
  Actions m_actions;
  QHash<QString, QAction *> m_appShellActionIds;
  EditorAppShellBridge m_appShellBridge;
  QPlainTextEdit *m_editor = nullptr;
  QWidget *m_externalBanner = nullptr;
  QLabel *m_externalLabel = nullptr;
  QPushButton *m_reloadButton = nullptr;
  QPushButton *m_saveAsButton = nullptr;
  ExternalState m_renderedExternalState = ExternalState::InSync;
  bool m_replacingContents = false;
  bool m_firstFramePublished = false;
  bool m_pendingCloseApproved = false;
};

} // namespace QindaQt::Apps::TextEditor
