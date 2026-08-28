// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"

#include "app_shell/editor_action_catalog.h"
#include "app_shell/native_file_selection_adapter.h"

#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStatusBar>
#include <QTextCursor>
#include <QTextDocument>
#include <QVBoxLayout>

#include <utility>

namespace QindaQt::Apps::TextEditor {
namespace {

[[nodiscard]] QString colorCss(const QColor &color) {
  return color.name(QColor::HexArgb);
}

} // namespace

EditorWindow::EditorWindow(
    DocumentStorePtr store, EditorAppearance appearance,
    std::unique_ptr<FileSelectionAdapter> fileSelectionAdapter,
    QWidget *parent)
    : QMainWindow(parent),
      m_controller(new DocumentController(std::move(store), this)),
      m_appearance(std::move(appearance)),
      m_appShellBridge(fileSelectionAdapter
                           ? std::move(fileSelectionAdapter)
                           : std::make_unique<NativeFileSelectionAdapter>(
                                 this),
                       this) {
  setObjectName(QStringLiteral("qindaqtEditorWindow"));
  setAccessibleName(tr("QindaQt Text Editor"));
  resize(920, 680);
  setMinimumSize(420, 320);
  createCentralSurface();
  createActions();
  createMenus();
  publishAppShellProjection();
  connectState();
  applyAppearance();
  updateDocumentPresentation();
  m_editor->setFocus(Qt::OtherFocusReason);
}

DocumentController *EditorWindow::controller() const { return m_controller; }
QPlainTextEdit *EditorWindow::editor() const { return m_editor; }
QindaQt::AppShell::ApplicationCoordinator &EditorWindow::appShellCoordinator() {
  return m_appShellBridge.coordinator();
}

void EditorWindow::createCentralSurface() {
  auto *surface = new QWidget(this);
  surface->setObjectName(QStringLiteral("editorSurface"));
  auto *layout = new QVBoxLayout(surface);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  m_externalBanner = new QWidget(surface);
  m_externalBanner->setObjectName(QStringLiteral("externalChangeBanner"));
  m_externalBanner->setAccessibleName(tr("External file change warning"));
  auto *bannerLayout = new QHBoxLayout(m_externalBanner);
  bannerLayout->setContentsMargins(12, 8, 12, 8);
  m_externalLabel = new QLabel(m_externalBanner);
  m_externalLabel->setObjectName(QStringLiteral("externalChangeMessage"));
  m_externalLabel->setWordWrap(true);
  m_externalLabel->setAccessibleName(tr("External file status"));
  m_reloadButton = new QPushButton(tr("&Reload"), m_externalBanner);
  m_reloadButton->setObjectName(QStringLiteral("reloadExternalAction"));
  m_reloadButton->setAccessibleName(tr("Reload file from disk"));
  m_saveAsButton = new QPushButton(tr("Save &As…"), m_externalBanner);
  m_saveAsButton->setObjectName(QStringLiteral("saveAsExternalAction"));
  m_saveAsButton->setAccessibleName(
      tr("Save this document under a different name"));
  bannerLayout->addWidget(m_externalLabel, 1);
  bannerLayout->addWidget(m_reloadButton);
  bannerLayout->addWidget(m_saveAsButton);
  m_externalBanner->hide();

  m_editor = new QPlainTextEdit(surface);
  m_editor->setObjectName(QStringLiteral("documentEditor"));
  m_editor->setAccessibleName(tr("Document text"));
  m_editor->setAccessibleDescription(
      tr("Edit the current local UTF-8 plain-text document"));
  m_editor->setTabChangesFocus(false);
  m_editor->setLineWrapMode(QPlainTextEdit::WidgetWidth);
  layout->addWidget(m_externalBanner);
  layout->addWidget(m_editor, 1);
  setCentralWidget(surface);
  setTabOrder(m_editor, m_reloadButton);
  setTabOrder(m_reloadButton, m_saveAsButton);
  setTabOrder(m_saveAsButton, m_editor);

  statusBar()->setObjectName(QStringLiteral("editorStatusBar"));
  statusBar()->showMessage(tr("Ready"));
}

void EditorWindow::createActions() {
  const auto addAction = [this](const QString &objectName, const QString &text,
                                const QKeySequence::StandardKey key,
                                const auto &handler) {
    auto *action = new QAction(text, this);
    action->setObjectName(objectName);
    action->setShortcut(QKeySequence(key));
    action->setShortcutContext(Qt::WindowShortcut);
    connect(action, &QAction::triggered, this, handler);
    return action;
  };

  m_actions.fileNew =
      addAction(QStringLiteral("fileNewAction"), tr("&New"), QKeySequence::New,
                [this] { newInteractively(); });
  m_actions.fileOpen =
      addAction(QStringLiteral("fileOpenAction"), tr("&Open…"),
                QKeySequence::Open, [this] { openInteractively(); });
  m_actions.fileSave =
      addAction(QStringLiteral("fileSaveAction"), tr("&Save"),
                QKeySequence::Save, [this] { (void)saveInteractively(); });
  m_actions.fileSaveAs =
      addAction(QStringLiteral("fileSaveAsAction"), tr("Save &As…"),
                QKeySequence::SaveAs, [this] { (void)saveAsInteractively(); });
  m_actions.fileQuit = addAction(QStringLiteral("fileQuitAction"), tr("&Quit"),
                                 QKeySequence::Quit, [this] { close(); });

  const auto addEditorAction =
      [this](const QString &objectName, const QString &text,
             const QKeySequence::StandardKey key, const auto &handler) {
        auto *action = new QAction(text, this);
        action->setObjectName(objectName);
        action->setShortcut(QKeySequence(key));
        action->setShortcutContext(Qt::WindowShortcut);
        connect(action, &QAction::triggered, m_editor, handler);
        return action;
      };
  m_actions.editUndo =
      addEditorAction(QStringLiteral("editUndoAction"), tr("&Undo"),
                      QKeySequence::Undo, &QPlainTextEdit::undo);
  m_actions.editRedo =
      addEditorAction(QStringLiteral("editRedoAction"), tr("&Redo"),
                      QKeySequence::Redo, &QPlainTextEdit::redo);
  m_actions.editCut =
      addEditorAction(QStringLiteral("editCutAction"), tr("Cu&t"),
                      QKeySequence::Cut, &QPlainTextEdit::cut);
  m_actions.editCopy =
      addEditorAction(QStringLiteral("editCopyAction"), tr("&Copy"),
                      QKeySequence::Copy, &QPlainTextEdit::copy);
  m_actions.editPaste =
      addEditorAction(QStringLiteral("editPasteAction"), tr("&Paste"),
                      QKeySequence::Paste, &QPlainTextEdit::paste);
  m_actions.editSelectAll =
      addEditorAction(QStringLiteral("editSelectAllAction"), tr("Select &All"),
                      QKeySequence::SelectAll, &QPlainTextEdit::selectAll);

  m_actions.editUndo->setEnabled(false);
  m_actions.editRedo->setEnabled(false);
  m_actions.editCut->setEnabled(false);
  m_actions.editCopy->setEnabled(false);
  m_actions.editPaste->setEnabled(m_editor->canPaste());
  connect(m_editor, &QPlainTextEdit::undoAvailable, m_actions.editUndo,
          &QAction::setEnabled);
  connect(m_editor, &QPlainTextEdit::redoAvailable, m_actions.editRedo,
          &QAction::setEnabled);
  connect(m_editor, &QPlainTextEdit::copyAvailable, m_actions.editCut,
          &QAction::setEnabled);
  connect(m_editor, &QPlainTextEdit::copyAvailable, m_actions.editCopy,
          &QAction::setEnabled);
  connect(QApplication::clipboard(), &QClipboard::dataChanged,
          m_actions.editPaste,
          [this] { m_actions.editPaste->setEnabled(m_editor->canPaste()); });

  m_appShellActionIds = {
      {QString::fromLatin1(AppShellActionIds::FileNew), m_actions.fileNew},
      {QString::fromLatin1(AppShellActionIds::FileOpen), m_actions.fileOpen},
      {QString::fromLatin1(AppShellActionIds::FileSave), m_actions.fileSave},
      {QString::fromLatin1(AppShellActionIds::FileSaveAs),
       m_actions.fileSaveAs},
      {QString::fromLatin1(AppShellActionIds::FileQuit), m_actions.fileQuit},
      {QString::fromLatin1(AppShellActionIds::EditUndo), m_actions.editUndo},
      {QString::fromLatin1(AppShellActionIds::EditRedo), m_actions.editRedo},
      {QString::fromLatin1(AppShellActionIds::EditCut), m_actions.editCut},
      {QString::fromLatin1(AppShellActionIds::EditCopy), m_actions.editCopy},
      {QString::fromLatin1(AppShellActionIds::EditPaste),
       m_actions.editPaste},
      {QString::fromLatin1(AppShellActionIds::EditSelectAll),
       m_actions.editSelectAll},
  };
}

void EditorWindow::createMenus() {
  auto *fileMenu = menuBar()->addMenu(tr("&File"));
  fileMenu->setObjectName(QStringLiteral("fileMenu"));
  fileMenu->addAction(m_actions.fileNew);
  fileMenu->addAction(m_actions.fileOpen);
  fileMenu->addSeparator();
  fileMenu->addAction(m_actions.fileSave);
  fileMenu->addAction(m_actions.fileSaveAs);
  fileMenu->addSeparator();
  fileMenu->addAction(m_actions.fileQuit);

  auto *editMenu = menuBar()->addMenu(tr("&Edit"));
  editMenu->setObjectName(QStringLiteral("editMenu"));
  editMenu->addAction(m_actions.editUndo);
  editMenu->addAction(m_actions.editRedo);
  editMenu->addSeparator();
  editMenu->addAction(m_actions.editCut);
  editMenu->addAction(m_actions.editCopy);
  editMenu->addAction(m_actions.editPaste);
  editMenu->addSeparator();
  editMenu->addAction(m_actions.editSelectAll);
}

void EditorWindow::publishAppShellProjection() {
  // AGENT-GUARD: This must run after createActions() so the local QAction
  // enabled states below are already settled, and before any presentation
  // update runs, so the published snapshot never lags the visible menu.
  const QindaQt::AppShell::Error published =
      m_appShellBridge.publishActionCatalog();
  Q_ASSERT(published.ok());
  Q_UNUSED(published);
  for (auto it = m_appShellActionIds.cbegin(); it != m_appShellActionIds.cend();
       ++it) {
    (void)m_appShellBridge.setActionEnabled(it.key(), it.value()->isEnabled());
  }
  m_appShellBridge.bindActivationTargets(m_appShellActionIds);
}

void EditorWindow::connectState() {
  connect(
      m_editor->document(), &QTextDocument::contentsChange, this,
      [this](const int position, const int charsRemoved, const int charsAdded) {
        if (m_replacingContents) {
          return;
        }
        QTextCursor addedText(m_editor->document());
        addedText.setPosition(position);
        addedText.setPosition(position + charsAdded, QTextCursor::KeepAnchor);
        QString insertedText = addedText.selectedText();
        insertedText.replace(QChar::ParagraphSeparator, u'\n');
        insertedText.replace(QChar::LineSeparator, u'\n');
        m_controller->applyTextEdit(position, charsRemoved,
                                    std::move(insertedText));
      });
  connect(m_controller, &DocumentController::contentsReplacementRequested, this,
          [this](const QString &text) {
            m_replacingContents = true;
            m_editor->setPlainText(text);
            m_editor->document()->setModified(false);
            m_replacingContents = false;
            m_editor->moveCursor(QTextCursor::Start);
            m_editor->setFocus(Qt::OtherFocusReason);
          });
  connect(m_controller, &DocumentController::stateChanged, this,
          &EditorWindow::updateDocumentPresentation);
  connect(m_controller, &DocumentController::externalStateChanged, this,
          &EditorWindow::announceExternalState);
  connect(m_reloadButton, &QPushButton::clicked, this,
          &EditorWindow::reloadInteractively);
  connect(m_saveAsButton, &QPushButton::clicked, this,
          [this] { (void)saveAsInteractively(); });

  for (auto it = m_appShellActionIds.cbegin(); it != m_appShellActionIds.cend();
       ++it) {
    connect(it.value(), &QAction::enabledChanged, this,
            [this, id = it.key()](bool enabled) {
              (void)m_appShellBridge.setActionEnabled(id, enabled);
            });
  }
  connect(&m_appShellBridge.coordinator(),
          &QindaQt::AppShell::ApplicationCoordinator::quitDecisionRequested,
          this, [this](quint64 requestId, const QString &) {
            (void)m_appShellBridge.coordinator().resolveQuit(
                requestId,
                confirmDiscardOrSave() == PendingAction::Continue);
          });
  connect(&m_appShellBridge.coordinator(),
          &QindaQt::AppShell::ApplicationCoordinator::quitApproved, this,
          [this](quint64) { m_pendingCloseApproved = true; });
}

void EditorWindow::applyAppearance() {
  setPalette(m_appearance.palette);
  setFont(m_appearance.interfaceFont);
  m_editor->setFont(m_appearance.editorFont);
}

void EditorWindow::applyExternalBannerAppearance(const ExternalState state) {
  const bool warning = state == ExternalState::Changed;
  const QColor background =
      warning ? m_appearance.warningBackground : m_appearance.dangerBackground;
  const QColor foreground =
      warning ? m_appearance.warningForeground : m_appearance.dangerForeground;
  m_externalBanner->setStyleSheet(
      QStringLiteral("#externalChangeBanner { background: %1; border-bottom: "
                     "1px solid %2; }"
                     "#externalChangeMessage { color: %3; }"
                     "#externalChangeBanner QPushButton:focus { border: 2px "
                     "solid %4; border-radius: %5px; }")
          .arg(colorCss(background), colorCss(foreground), colorCss(foreground),
               colorCss(m_appearance.focusRing),
               QString::number(m_appearance.mediumRadius)));
}

void EditorWindow::updateDocumentPresentation() {
  const DocumentState &state = m_controller->state();
  const QString name =
      state.isUntitled() ? tr("Untitled") : QFileInfo(state.path()).fileName();
  setWindowTitle(tr("%1[*] — QindaQt Text Editor").arg(name));
  setWindowFilePath(state.path());
  setWindowModified(state.isDirty());
  m_editor->document()->setModified(state.isDirty());
  m_actions.fileSave->setEnabled(state.isDirty());
  updateExternalBanner(state.externalState());
  if (state.externalState() == ExternalState::Changed) {
    statusBar()->showMessage(tr("File changed outside the editor"));
  } else if (state.externalState() == ExternalState::Missing) {
    statusBar()->showMessage(tr("File was removed outside the editor"));
  } else if (state.externalState() == ExternalState::Unreadable) {
    statusBar()->showMessage(tr("File can no longer be checked"));
  } else if (state.isDirty()) {
    statusBar()->showMessage(tr("Modified"));
  } else if (state.isUntitled()) {
    statusBar()->showMessage(tr("Ready"));
  } else {
    statusBar()->showMessage(tr("Saved"));
  }
}

void EditorWindow::updateExternalBanner(const ExternalState state) {
  if (m_renderedExternalState == state) {
    return;
  }
  m_renderedExternalState = state;

  QString text;
  switch (state) {
  case ExternalState::InSync: {
    QWidget *focused = QApplication::focusWidget();
    const bool recoveryHadFocus =
        focused != nullptr && (focused == m_externalBanner ||
                               m_externalBanner->isAncestorOf(focused));
    m_externalBanner->hide();
    if (recoveryHadFocus) {
      m_editor->setFocus(Qt::OtherFocusReason);
    }
    return;
  }
  case ExternalState::Changed:
    text = tr("Warning: This file changed outside QindaQt Text Editor. Reload "
              "it or save your text under a different name.");
    break;
  case ExternalState::Missing:
    text = tr("Error: This file was removed outside QindaQt Text Editor. Save "
              "your text under a different name.");
    break;
  case ExternalState::Unreadable:
    text = tr("Error: This file can no longer be checked. Saving over it is "
              "blocked; use Save As.");
    break;
  }
  applyExternalBannerAppearance(state);
  m_externalLabel->setText(text);
  m_externalLabel->setAccessibleDescription(text);
  m_reloadButton->setEnabled(state == ExternalState::Changed);
  m_externalBanner->show();
}

void EditorWindow::announceExternalState(const ExternalState state) {
  if (state == ExternalState::InSync) {
    return;
  }
  QAccessibleAnnouncementEvent event(m_externalLabel, m_externalLabel->text());
  event.setPoliteness(QAccessible::AnnouncementPoliteness::Assertive);
  QAccessible::updateAccessibility(&event);
}

EditorWindow::PendingAction EditorWindow::confirmDiscardOrSave() {
  if (!m_controller->state().isDirty()) {
    return PendingAction::Continue;
  }
  const QMessageBox::StandardButton choice = QMessageBox::warning(
      this, tr("Unsaved changes"), tr("Save changes before continuing?"),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  if (choice == QMessageBox::Cancel) {
    return PendingAction::Cancel;
  }
  if (choice == QMessageBox::Save && !saveInteractively()) {
    return PendingAction::Cancel;
  }
  return PendingAction::Continue;
}

bool EditorWindow::saveInteractively() {
  if (m_controller->state().isUntitled()) {
    return saveAsInteractively();
  }
  const DocumentOperation result = m_controller->save();
  if (!result.ok()) {
    showOperationError(result.error == DocumentError::ExternalConflict
                           ? tr("File changed outside the editor")
                           : tr("Could not save document"),
                       result);
    return false;
  }
  return true;
}

bool EditorWindow::saveAsInteractively() {
  // AGENT-CONTRACT: File selection is mediated by AppShell's fail-closed
  // portal request (docs/wiki/apps/application-shell.md), so the suggested
  // name is a bare base name rather than a full initial path.
  const QString suggestedName =
      m_controller->state().isUntitled()
          ? QString()
          : QFileInfo(m_controller->state().path()).fileName();
  const std::optional<QString> path =
      m_appShellBridge.requestSaveFile(suggestedName);
  if (!path) {
    return false;
  }

  DocumentOperation result = m_controller->saveAs(*path, false);
  if (result.error == DocumentError::DestinationExists) {
    const auto answer = QMessageBox::question(
        this, tr("Replace existing file?"),
        tr("A file with this name already exists. Replace its contents?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      return false;
    }
    result = m_controller->saveAs(*path, true);
  }
  if (!result.ok()) {
    showOperationError(tr("Could not save document"), result);
    return false;
  }
  return true;
}

void EditorWindow::openInteractively() {
  if (confirmDiscardOrSave() == PendingAction::Cancel) {
    return;
  }
  const std::optional<QString> path = m_appShellBridge.requestOpenFile();
  if (!path) {
    return;
  }
  const DocumentOperation result = m_controller->openPath(*path);
  if (!result.ok()) {
    showOperationError(tr("Could not open document"), result);
  }
}

void EditorWindow::newInteractively() {
  if (confirmDiscardOrSave() == PendingAction::Continue) {
    m_controller->newDocument();
  }
}

void EditorWindow::reloadInteractively() {
  if (confirmDiscardOrSave() == PendingAction::Cancel) {
    return;
  }
  const DocumentOperation result =
      m_controller->openPath(m_controller->state().path());
  if (!result.ok()) {
    showOperationError(tr("Could not reload document"), result);
  }
}

void EditorWindow::showOperationError(const QString &title,
                                      const DocumentOperation &result) {
  QMessageBox::critical(this, title,
                        result.diagnostic.isEmpty() ? tr("The operation failed")
                                                    : result.diagnostic);
}

void EditorWindow::closeEvent(QCloseEvent *event) {
  // AGENT-GUARD: Close consent is mediated by AppShell's quit lifecycle
  // (docs/wiki/apps/application-shell.md). requestQuit() synchronously emits
  // quitDecisionRequested, whose connectState() handler runs
  // confirmDiscardOrSave() and calls resolveQuit() before requestQuit()
  // returns here — that nested resolveQuit() already clears the
  // coordinator's pending ID, so requestQuit()'s own return value is stale
  // by the time we see it and must not be used as the approval signal. Read
  // the decision only from m_pendingCloseApproved, set by the quitApproved
  // handler below.
  m_pendingCloseApproved = false;
  (void)m_appShellBridge.coordinator().requestQuit(
      QStringLiteral("window-close"));
  if (m_pendingCloseApproved) {
    event->accept();
  } else {
    event->ignore();
  }
}

void EditorWindow::paintEvent(QPaintEvent *event) {
  QMainWindow::paintEvent(event);
  if (!m_firstFramePublished) {
    m_firstFramePublished = true;
    emit firstFramePainted();
  }
}

} // namespace QindaQt::Apps::TextEditor
