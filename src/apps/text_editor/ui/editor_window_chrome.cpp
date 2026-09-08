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
  toolbar->setIconSize(QSize(22, 22));
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
  for (auto *action : findChildren<QAction *>()) {
    const auto name = action->property("editorIconName").toString();
    if (!name.isEmpty())
      action->setIcon(
          editorIcon(name, m_appearance.palette.color(QPalette::WindowText)));
  }
  const auto css = [](const QColor &color) {
    return color.name(QColor::HexArgb);
  };
  // AGENT-GUARD: The canvas remains opaque. Chrome colors, hover and focus
  // consume semantic values; never derive a competing per-application palette.
  setStyleSheet(
      QStringLiteral(
          "QToolBar#documentToolbar { border: 0; spacing: 4px; padding: 8px "
          "12px; "
          "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %1,stop:1 "
          "%2); }"
          "QToolBar#documentToolbar QToolButton { border: 2px solid "
          "transparent; "
          "border-radius: %3px; padding: 6px; }"
          "QToolBar#documentToolbar QToolButton:hover { background: %4; }"
          "QToolBar#documentToolbar QToolButton:pressed { background: %5; }"
          "QToolBar#documentToolbar QToolButton:focus { border-color: %6; }"
          "QStatusBar#editorStatusBar { border: 0; padding: 6px 12px; color: "
          "%7; }"
          "QStatusBar::item { border: 0; }"
          "QWidget#findReplaceBar { border-top: 1px solid %8; }"
          "QLineEdit { padding: 5px; border: 1px solid %8; border-radius: "
          "%3px; }"
          "QLineEdit:focus { border: 1px solid %6; }"
          "QPlainTextEdit#documentEditor { border: 1px solid %8; "
          "border-radius: %3px; }"
          "QPlainTextEdit#documentEditor:focus { border: 1px solid %6; }")
          .arg(css(m_appearance.palette.color(QPalette::AlternateBase)),
               css(m_appearance.palette.color(QPalette::Window)),
               QString::number(m_appearance.mediumRadius),
               css(m_appearance.hover), css(m_appearance.pressed),
               css(m_appearance.focusRing),
               css(m_appearance.palette.color(QPalette::WindowText)),
               css(m_appearance.divider)));
  statusBar()->setSizeGripEnabled(false);
}
} // namespace QindaQt::Apps::TextEditor
