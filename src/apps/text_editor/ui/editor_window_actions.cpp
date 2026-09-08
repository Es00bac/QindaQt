// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"
#include "editor_application.h"

#include "app_shell/editor_action_catalog.h"
#include "restore/text_editor_restore_policy.h"
#include "ui/find_replace_bar.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFileInfo>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QStatusBar>

namespace QindaQt::Apps::TextEditor {

void EditorWindow::createActions() {
  const auto add = [this](const QString &name, const QString &text,
                          const QKeySequence &shortcut,
                          const std::function<void()> &handler) {
    auto *action = new QAction(text, this);
    action->setObjectName(name);
    action->setShortcut(shortcut);
    action->setShortcutContext(Qt::WindowShortcut);
    connect(action, &QAction::triggered, this, handler);
    return action;
  };

  m_actions.fileNew =
      add(QStringLiteral("fileNewAction"), tr("&New"),
          QKeySequence(QKeySequence::New), [this] { newInteractively(); });
  m_actions.fileOpen =
      add(QStringLiteral("fileOpenAction"), tr("&Open…"),
          QKeySequence(QKeySequence::Open), [this] { openInteractively(); });
  m_actions.fileCloseWindow =
      add(QStringLiteral("fileCloseWindowAction"), tr("&Close Window"),
          QKeySequence(QStringLiteral("Ctrl+W")),
          [this] { close(); });
  m_actions.fileSave = add(QStringLiteral("fileSaveAction"), tr("&Save"),
                           QKeySequence(QKeySequence::Save), [this] {
                             (void)saveDocument();
                           });
  m_actions.fileSaveAs =
      add(QStringLiteral("fileSaveAsAction"), tr("Save &As…"),
          QKeySequence(QKeySequence::SaveAs),
          [this] { (void)saveDocumentAs(); });
  m_actions.fileQuit =
      add(QStringLiteral("fileQuitAction"), tr("&Quit"),
          QKeySequence(QKeySequence::Quit), [this] { close(); });

  const auto editorCommand = [this](auto command) {
    if (QPlainTextEdit *active = editor()) {
      (active->*command)();
    }
  };
  m_actions.editUndo = add(QStringLiteral("editUndoAction"), tr("&Undo"),
                           QKeySequence(QKeySequence::Undo), [editorCommand] {
                             editorCommand(&QPlainTextEdit::undo);
                           });
  m_actions.editRedo = add(QStringLiteral("editRedoAction"), tr("&Redo"),
                           QKeySequence(QKeySequence::Redo), [editorCommand] {
                             editorCommand(&QPlainTextEdit::redo);
                           });
  m_actions.editCut = add(QStringLiteral("editCutAction"), tr("Cu&t"),
                          QKeySequence(QKeySequence::Cut), [editorCommand] {
                            editorCommand(&QPlainTextEdit::cut);
                          });
  m_actions.editCopy = add(QStringLiteral("editCopyAction"), tr("&Copy"),
                           QKeySequence(QKeySequence::Copy), [editorCommand] {
                             editorCommand(&QPlainTextEdit::copy);
                           });
  m_actions.editPaste = add(QStringLiteral("editPasteAction"), tr("&Paste"),
                            QKeySequence(QKeySequence::Paste), [editorCommand] {
                              editorCommand(&QPlainTextEdit::paste);
                            });
  m_actions.editSelectAll =
      add(QStringLiteral("editSelectAllAction"), tr("Select &All"),
          QKeySequence(QKeySequence::SelectAll),
          [editorCommand] { editorCommand(&QPlainTextEdit::selectAll); });
  m_actions.editFind =
      add(QStringLiteral("editFindAction"), tr("&Find…"),
          QKeySequence(QKeySequence::Find), [this] { openFind(false); });
  m_actions.editReplace =
      add(QStringLiteral("editReplaceAction"), tr("&Replace…"),
          QKeySequence(QStringLiteral("Ctrl+H")), [this] { openFind(true); });
  m_actions.editFindNext =
      add(QStringLiteral("editFindNextAction"), tr("Find &Next"),
          QKeySequence(QStringLiteral("F3")),
          [this] { findMatch(FindDirection::Next); });
  m_actions.editFindPrevious =
      add(QStringLiteral("editFindPreviousAction"), tr("Find &Previous"),
          QKeySequence(QStringLiteral("Shift+F3")),
          [this] { findMatch(FindDirection::Previous); });
  m_actions.editFindClose =
      add(QStringLiteral("editFindCloseAction"), tr("Close Find"),
          QKeySequence(QStringLiteral("Escape")), [this] {
            if (m_findBar->isVisible()) {
              m_findBar->closeBar();
            }
          });

  m_actions.restoreDocuments = add(
      QStringLiteral("restoreDocumentsAction"), tr("Restore Open &Documents"),
      QKeySequence(QStringLiteral("Ctrl+Alt+R")), [this] {
        if (!m_restorePolicy ||
            !m_restorePolicy->requestEnabled(!m_restorePolicy->enabled())) {
          announceStatus(tr("Restore policy is unavailable or busy"));
        }
      });
  m_actions.restoreDocuments->setCheckable(true);

  connect(QApplication::clipboard(), &QClipboard::dataChanged, this,
          &EditorWindow::updateActionStates);
  connect(m_findBar, &FindReplaceBar::findNextRequested, this,
          [this] { findMatch(FindDirection::Next); });
  connect(m_findBar, &FindReplaceBar::findPreviousRequested, this,
          [this] { findMatch(FindDirection::Previous); });
  connect(m_findBar, &FindReplaceBar::replaceRequested, this,
          &EditorWindow::replaceCurrent);
  connect(m_findBar, &FindReplaceBar::replaceAllRequested, this,
          &EditorWindow::replaceAll);

  m_appShellActionIds = {
      {QString::fromLatin1(AppShellActionIds::FileNew), m_actions.fileNew},
      {QString::fromLatin1(AppShellActionIds::FileOpen), m_actions.fileOpen},
      {QString::fromLatin1(AppShellActionIds::FileCloseWindow),
       m_actions.fileCloseWindow},
      {QString::fromLatin1(AppShellActionIds::FileSave), m_actions.fileSave},
      {QString::fromLatin1(AppShellActionIds::FileSaveAs),
       m_actions.fileSaveAs},
      {QString::fromLatin1(AppShellActionIds::FileQuit), m_actions.fileQuit},
      {QString::fromLatin1(AppShellActionIds::EditUndo), m_actions.editUndo},
      {QString::fromLatin1(AppShellActionIds::EditRedo), m_actions.editRedo},
      {QString::fromLatin1(AppShellActionIds::EditCut), m_actions.editCut},
      {QString::fromLatin1(AppShellActionIds::EditCopy), m_actions.editCopy},
      {QString::fromLatin1(AppShellActionIds::EditPaste), m_actions.editPaste},
      {QString::fromLatin1(AppShellActionIds::EditSelectAll),
       m_actions.editSelectAll},
      {QString::fromLatin1(AppShellActionIds::EditFind), m_actions.editFind},
      {QString::fromLatin1(AppShellActionIds::EditReplace),
       m_actions.editReplace},
      {QString::fromLatin1(AppShellActionIds::EditFindNext),
       m_actions.editFindNext},
      {QString::fromLatin1(AppShellActionIds::EditFindPrevious),
       m_actions.editFindPrevious},
      {QString::fromLatin1(AppShellActionIds::EditFindClose),
       m_actions.editFindClose},
      {QString::fromLatin1(AppShellActionIds::RestoreDocuments),
       m_actions.restoreDocuments},
  };
  createEditingActions();
  updateActionStates();
}

void EditorWindow::createMenus() {
  auto *file = menuBar()->addMenu(tr("&File"));
  file->setObjectName(QStringLiteral("fileMenu"));
  file->addActions(
      {m_actions.fileNew, m_actions.fileOpen, m_actions.fileCloseWindow});
  file->addSeparator();
  file->addActions({m_actions.fileSave, m_actions.fileSaveAs});
  file->addSeparator();
  file->addAction(m_actions.fileQuit);

  auto *edit = menuBar()->addMenu(tr("&Edit"));
  edit->setObjectName(QStringLiteral("editMenu"));
  edit->addActions({m_actions.editUndo, m_actions.editRedo});
  edit->addSeparator();
  edit->addActions({m_actions.editCut, m_actions.editCopy, m_actions.editPaste,
                    m_actions.editSelectAll});
  edit->addSeparator();
  edit->addActions({m_actions.editFind, m_actions.editReplace,
                    m_actions.editFindNext, m_actions.editFindPrevious});

  edit->addSeparator();
  edit->addActions(m_actions.editingTools);
  createViewMenu();

  auto *settings = menuBar()->addMenu(tr("&Settings"));
  settings->setObjectName(QStringLiteral("settingsMenu"));
  settings->addAction(m_actions.restoreDocuments);
}

void EditorWindow::publishAppShellProjection() {
  const QindaQt::AppShell::Error published =
      m_appShellBridge.publishActionCatalog();
  Q_ASSERT(published.ok());
  Q_UNUSED(published);
  for (auto it = m_appShellActionIds.cbegin(); it != m_appShellActionIds.cend();
       ++it) {
    (void)m_appShellBridge.setActionEnabled(it.key(), it.value()->isEnabled());
    connect(it.value(), &QAction::enabledChanged, this,
            [this, id = it.key()](bool enabled) {
              (void)m_appShellBridge.setActionEnabled(id, enabled);
            });
    if (it.value()->isCheckable()) {
      (void)m_appShellBridge.coordinator().setActionChecked(
          it.key(), it.value()->isChecked());
      connect(it.value(), &QAction::toggled, this,
              [this, id = it.key()](bool checked) {
                (void)m_appShellBridge.coordinator().setActionChecked(id,
                                                                      checked);
              });
    }
  }
  m_appShellBridge.bindActivationTargets(m_appShellActionIds);
}

void EditorWindow::updateActionStates() {
  QPlainTextEdit *activeEditor = editor();
  DocumentController *activeDocument = controller();
  const bool hasDocument = activeEditor && activeDocument;
  m_actions.fileCloseWindow->setEnabled(hasDocument);
  m_actions.fileSave->setEnabled(hasDocument &&
                                 activeDocument->state().isDirty());
  m_actions.fileSaveAs->setEnabled(hasDocument);
  m_actions.editUndo->setEnabled(hasDocument &&
                                 activeEditor->document()->isUndoAvailable());
  m_actions.editRedo->setEnabled(hasDocument &&
                                 activeEditor->document()->isRedoAvailable());
  const bool selection =
      hasDocument && activeEditor->textCursor().hasSelection();
  m_actions.editCut->setEnabled(selection);
  m_actions.editCopy->setEnabled(selection);
  m_actions.editPaste->setEnabled(hasDocument && activeEditor->canPaste());
  m_actions.editSelectAll->setEnabled(hasDocument &&
                                      !activeEditor->document()->isEmpty());
  m_actions.editFindNext->setEnabled(hasDocument && m_findBar->isVisible());
  m_actions.editFindPrevious->setEnabled(hasDocument && m_findBar->isVisible());
  m_actions.editFindClose->setEnabled(m_findBar->isVisible());
  for (auto *action : m_actions.editingTools) action->setEnabled(hasDocument);
  for (auto *action : m_actions.viewTools) action->setEnabled(hasDocument);
  if (!m_actions.viewTools.isEmpty())
    m_actions.viewTools.first()->setChecked(hasDocument && activeEditor->lineWrapMode() != QPlainTextEdit::NoWrap);
}

void EditorWindow::newInteractively() {
  if (m_application) (void)m_application->newWindow();
}

void EditorWindow::openInteractively() {
  const std::optional<QString> path = m_appShellBridge.requestOpenFile();
  if (!path) {
    return;
  }
  QString diagnostic;
  if (!addPath(*path, &diagnostic)) {
    showOperationError(
        tr("Could not open document"),
        {.error = DocumentError::ReadFailed, .diagnostic = diagnostic});
    return;
  }
  persistRestoreState();
}

bool EditorWindow::saveDocument() {
  DocumentController *document = m_document;
  if (!document) {
    return false;
  }
  if (document->state().isUntitled()) {
    return saveDocumentAs();
  }
  const DocumentOperation result = document->save();
  if (!result.ok()) {
    showOperationError(result.error == DocumentError::ExternalConflict
                           ? tr("File changed outside the editor")
                           : tr("Could not save document"),
                       result);
    return false;
  }
  persistRestoreState();
  return true;
}

bool EditorWindow::saveDocumentAs() {
  DocumentController *document = m_document;
  if (!document) {
    return false;
  }
  const QString suggested =
      document->state().isUntitled()
          ? QString()
          : QFileInfo(document->state().path()).fileName();
  const std::optional<QString> path =
      m_appShellBridge.requestSaveFile(suggested);
  if (!path) {
    return false;
  }
  if (m_application && m_application->windowForPath(*path) &&
      m_application->windowForPath(*path) != this) {
    showOperationError(tr("Could not save document"),
        {.error = DocumentError::AlreadyOpen,
         .diagnostic = tr("That file is already open in another window")});
    return false;
  }
  DocumentOperation result = document->saveAs(*path, false);
  if (result.error == DocumentError::DestinationExists) {
    if (!m_dialogs->confirmReplace()) {
      return false;
    }
    result = document->saveAs(*path, true);
  }
  if (!result.ok()) {
    showOperationError(tr("Could not save document"), result);
    return false;
  }
  updateDocumentPresentation(document);
  persistRestoreState();
  return true;
}

void EditorWindow::reloadInteractively() {
  DocumentController *document = m_document;
  if (!document || confirmDocumentClose() == PendingAction::Cancel) {
    return;
  }
  const DocumentOperation result = document->openPath(document->state().path());
  if (!result.ok()) {
    showOperationError(tr("Could not reload document"), result);
  }
}

void EditorWindow::showOperationError(const QString &title,
                                      const DocumentOperation &result) {
  m_dialogs->operationError(title,
                        result.diagnostic.isEmpty() ? tr("The operation failed")
                                                    : result.diagnostic);
}

} // namespace QindaQt::Apps::TextEditor
