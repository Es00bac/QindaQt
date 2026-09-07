// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_window.h"

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
#include <QMessageBox>
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
  m_actions.fileCloseTab =
      add(QStringLiteral("fileCloseTabAction"), tr("&Close Tab"),
          QKeySequence(QStringLiteral("Ctrl+W")),
          [this] { closeTab(m_tabs->currentIndex()); });
  m_actions.fileSave = add(QStringLiteral("fileSaveAction"), tr("&Save"),
                           QKeySequence(QKeySequence::Save), [this] {
                             (void)saveDocument(m_tabs->currentIndex());
                           });
  m_actions.fileSaveAs =
      add(QStringLiteral("fileSaveAsAction"), tr("Save &As…"),
          QKeySequence(QKeySequence::SaveAs),
          [this] { (void)saveDocumentAs(m_tabs->currentIndex()); });
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

  m_actions.tabNext = add(QStringLiteral("tabNextAction"), tr("Next Tab"),
                          QKeySequence(QStringLiteral("Ctrl+Tab")),
                          [this] { selectRelativeTab(1); });
  m_actions.tabPrevious =
      add(QStringLiteral("tabPreviousAction"), tr("Previous Tab"),
          QKeySequence(QStringLiteral("Ctrl+Shift+Tab")),
          [this] { selectRelativeTab(-1); });
  for (int index = 0; index < 9; ++index) {
    m_actions.tabSelect.append(
        add(QStringLiteral("tabSelect%1Action").arg(index + 1),
            tr("Select Tab %1").arg(index + 1),
            QKeySequence(QStringLiteral("Ctrl+%1").arg(index + 1)),
            [this, index] { selectTab(index); }));
  }

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
      {QString::fromLatin1(AppShellActionIds::FileCloseTab),
       m_actions.fileCloseTab},
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
      {QString::fromLatin1(AppShellActionIds::TabNext), m_actions.tabNext},
      {QString::fromLatin1(AppShellActionIds::TabPrevious),
       m_actions.tabPrevious},
      {QString::fromLatin1(AppShellActionIds::RestoreDocuments),
       m_actions.restoreDocuments},
  };
  for (int index = 0; index < m_actions.tabSelect.size(); ++index) {
    m_appShellActionIds.insert(QStringLiteral("tabs.select-%1").arg(index + 1),
                               m_actions.tabSelect.at(index));
  }
  createEditingActions();
  updateActionStates();
}

void EditorWindow::createMenus() {
  auto *file = menuBar()->addMenu(tr("&File"));
  file->setObjectName(QStringLiteral("fileMenu"));
  file->addActions(
      {m_actions.fileNew, m_actions.fileOpen, m_actions.fileCloseTab});
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

  auto *tabs = menuBar()->addMenu(tr("&Tabs"));
  tabs->setObjectName(QStringLiteral("tabsMenu"));
  tabs->addActions({m_actions.tabNext, m_actions.tabPrevious});
  tabs->addSeparator();
  tabs->addActions(m_actions.tabSelect);

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
  m_actions.fileCloseTab->setEnabled(hasDocument);
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
  const bool multiple = m_documents->count() > 1;
  m_actions.tabNext->setEnabled(multiple);
  m_actions.tabPrevious->setEnabled(multiple);
  for (int index = 0; index < m_actions.tabSelect.size(); ++index) {
    m_actions.tabSelect.at(index)->setEnabled(index < m_documents->count());
  }
}

void EditorWindow::selectRelativeTab(const int delta) {
  if (m_tabs->count() < 2) {
    return;
  }
  selectTab((m_tabs->currentIndex() + delta + m_tabs->count()) %
            m_tabs->count());
}

void EditorWindow::selectTab(const int index) {
  if (index >= 0 && index < m_tabs->count()) {
    m_tabs->setCurrentIndex(index);
  }
}

void EditorWindow::newInteractively() {
  const AddDocumentResult result = m_documents->addUntitled();
  if (!result.ok()) {
    showOperationError(tr("Could not create document"), result.operation);
  }
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

bool EditorWindow::saveDocument(const int index) {
  DocumentController *document = m_documents->at(index);
  if (!document) {
    return false;
  }
  if (document->state().isUntitled()) {
    return saveDocumentAs(index);
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

bool EditorWindow::saveDocumentAs(const int index) {
  DocumentController *document = m_documents->at(index);
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
  DocumentOperation result = m_documents->saveAs(index, *path, false);
  if (result.error == DocumentError::DestinationExists) {
    const auto answer = QMessageBox::question(
        this, tr("Replace existing file?"),
        tr("A file with this name already exists. Replace its contents?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
      return false;
    }
    result = m_documents->saveAs(index, *path, true);
  }
  if (!result.ok()) {
    showOperationError(tr("Could not save document"), result);
    return false;
  }
  updateDocumentPresentation(document);
  persistRestoreState();
  return true;
}

void EditorWindow::reloadInteractively(const int index) {
  DocumentController *document = m_documents->at(index);
  if (!document || confirmDocumentClose(index) == PendingAction::Cancel) {
    return;
  }
  const DocumentOperation result = document->openPath(document->state().path());
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

} // namespace QindaQt::Apps::TextEditor
