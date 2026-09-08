// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_chrome.h"
#include "qindaqt/design_tokens/design_tokens.h"

namespace QindaQt::Apps::Terminal {
QString terminalChromeStyleSheet(const QindaQt::DesignTokens::DesignTokens &t) {
  const QString raised = t.background().raised.name(QColor::HexArgb);
  const QString base = t.background().base.name(QColor::HexArgb);
  const QString text = t.foreground().defaultColor.name(QColor::HexArgb);
  const QString outline = t.strongOutline().name(QColor::HexArgb);
  const QString focus = t.focusRing().name(QColor::HexArgb);
  const QString hover = t.state().hover.name(QColor::HexArgb);
  const QString accent = t.accent().defaultColor.name(QColor::HexArgb);
  const QString accentInk = t.accent().foreground.name(QColor::HexArgb);
  const QString radius = QString::number(t.radius().small);
  return QStringLiteral(
      "QWidget#terminalFindBar, QStatusBar { background: %1; }"
      "QStatusBar { border-top: 1px solid %4; padding: 4px 10px; }"
      "QStatusBar::item { border: none; }"
      "QWidget#terminalFindBar QLineEdit, QDialog#terminalProfileDialog QLineEdit,"
      "QDialog#terminalProfileDialog QPlainTextEdit,"
      "QDialog#terminalProfileDialog QSpinBox, QDialog#terminalProfileDialog QComboBox {"
      " background: %2; color: %3; border: 1px solid %4; border-radius: %7px; padding: 6px; }"
      "QWidget#terminalFindBar QLineEdit:focus, QDialog#terminalProfileDialog QLineEdit:focus,"
      "QDialog#terminalProfileDialog QPlainTextEdit:focus, QDialog#terminalProfileDialog QSpinBox:focus,"
      "QDialog#terminalProfileDialog QComboBox:focus { border: 2px solid %5; padding: 5px; }"
      "QWidget#terminalFindBar QToolButton { border: 1px solid transparent;"
      " border-radius: %7px; padding: 6px; }"
      "QWidget#terminalFindBar QToolButton:hover { background: %6; }"
      "QWidget#terminalFindBar QToolButton:checked { background: %8; color: %9; font-weight: bold; }"
      "QWidget#terminalFindBar QToolButton:focus { border: 1px solid %5; }"
      "QDialog#terminalProfileDialog QListWidget { border: none; background: %1; padding: 6px; }"
      "QDialog#terminalProfileDialog QListWidget::item { padding: 10px; border-radius: %7px; }"
      "QDialog#terminalProfileDialog QPushButton { padding: 6px 12px; }")
      .arg(raised, base, text, outline, focus, hover, radius, accent, accentInk);
}
}
