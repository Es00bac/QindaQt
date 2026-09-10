// SPDX-License-Identifier: GPL-3.0-or-later
#include "editor_icon.h"
#include "editor_window.h"
#include "find_replace_bar.h"
#include "qindaqt/controls/application_icon.h"

#include <QAction>
#include <QLabel>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>

namespace QindaQt::Apps::TextEditor {
void EditorWindow::createToolbar() {
  auto *toolbar = addToolBar(tr("Document tools"));
  toolbar->setObjectName(QStringLiteral("documentToolbar"));
  toolbar->setMovable(false);
  toolbar->setFloatable(false);
  toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
  const auto add = [toolbar](QAction *action, const char *icon) {
    action->setProperty("editorIconName", QString::fromLatin1(icon));
    toolbar->addAction(action);
  };
  add(m_actions.fileNew, "document-new");
  add(m_actions.fileOpen, "document-open");
  toolbar->addSeparator();
  add(m_actions.fileSave, "document-save");
  add(m_actions.fileSaveAs, "document-save-as");
  auto *space = new QWidget(toolbar);
  space->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  toolbar->addWidget(space);
  add(m_actions.editFind, "edit-find");
  for (auto *button : toolbar->findChildren<QToolButton *>()) {
    button->setFocusPolicy(Qt::StrongFocus);
    if (auto *action = button->defaultAction()) {
      QString label = action->text();
      label.remove(QLatin1Char('&'));
      button->setAccessibleName(label);
      button->setToolTip(label + QStringLiteral("  ") +
                         action->shortcut().toString());
    }
  }
  setWindowIcon(QindaQt::Controls::applicationIcon(
      QStringLiteral("org.qindaqt.TextEditor")));
}

void EditorWindow::applyChrome() {
  m_findBar->refreshIcons();
  setWindowIcon(QindaQt::Controls::applicationIcon(
      QStringLiteral("org.qindaqt.TextEditor")));
  // Symbolic action icons are masks tinted with the live window-text role;
  // they are the one palette-derived presentation this window keeps.
  for (auto *action : findChildren<QAction *>()) {
    const auto name = action->property("editorIconName").toString();
    if (!name.isEmpty())
      action->setIcon(editorIcon(name, palette().color(QPalette::WindowText)));
  }
  // AGENT-CONTRACT: The native QStyle (Fusion over the Qt platform theme)
  // draws all chrome from the application palette. This window must never
  // install a stylesheet or a per-application palette (ADR-0116).
  statusBar()->setSizeGripEnabled(false);
}
} // namespace QindaQt::Apps::TextEditor
