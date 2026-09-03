// SPDX-License-Identifier: GPL-3.0-or-later
#include "links/terminal_link_opener.h"

#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>

namespace QindaQt::Apps::Terminal {

TerminalLinkOpener::TerminalLinkOpener(
    QString absoluteProgram, TerminalLinkSpawner *spawner,
    TerminalLinkConfirmation *confirmation)
    : m_absoluteProgram(std::move(absoluteProgram)), m_spawner(spawner),
      m_confirmation(confirmation) {}

TerminalLinkOpenResult TerminalLinkOpener::open(const TerminalLink &link,
                                                QWidget *parent) const {
  if (!isAdmittedTerminalLink(link)) {
    return {.diagnostic = QStringLiteral("The selected target is invalid")};
  }
  const QFileInfo program(m_absoluteProgram);
  if (!program.isAbsolute() || !program.isFile() || !program.isExecutable() ||
      m_spawner == nullptr || m_confirmation == nullptr) {
    return {.diagnostic = QStringLiteral("xdg-open is unavailable")};
  }
  if (!m_confirmation->confirm(link, parent)) {
    return {.cancelled = true,
            .diagnostic = QStringLiteral("Opening was cancelled")};
  }
  QString diagnostic;
  if (!m_spawner->spawn(program.absoluteFilePath(), QStringList{link.target},
                        &diagnostic)) {
    return {.diagnostic = diagnostic.isEmpty()
                              ? QStringLiteral("The opener could not start")
                              : diagnostic.left(512)};
  }
  return {.started = true, .cancelled = false, .diagnostic = {}};
}

QString TerminalLinkOpener::resolveXdgOpen() {
  const QString resolved = QStandardPaths::findExecutable(
      QStringLiteral("xdg-open"));
  const QFileInfo info(resolved);
  return info.isAbsolute() && info.isFile() && info.isExecutable()
             ? info.absoluteFilePath()
             : QString();
}

bool DetachedTerminalLinkSpawner::spawn(const QString &absoluteProgram,
                                        const QStringList &arguments,
                                        QString *diagnostic) {
  // AGENT-CONTRACT: xdg-open is a desktop-dispatch request, not a terminal
  // child. It is intentionally detached with one bounded argv target and no
  // PTY/process-group ownership; the selected handler owns its independent
  // lifetime. Never replace this with a shell string.
  const bool started = QProcess::startDetached(absoluteProgram, arguments);
  if (!started && diagnostic != nullptr) {
    *diagnostic = QStringLiteral("xdg-open could not be started");
  }
  return started;
}

bool MessageBoxTerminalLinkConfirmation::confirm(const TerminalLink &link,
                                                 QWidget *parent) {
  QMessageBox confirmation(parent);
  confirmation.setIcon(QMessageBox::Question);
  confirmation.setWindowTitle(QStringLiteral("Open terminal target"));
  confirmation.setTextFormat(Qt::PlainText);
  confirmation.setText(link.kind == TerminalLinkKind::WebUrl
                           ? QStringLiteral("Open this web address?")
                           : QStringLiteral("Open this local path?"));
  confirmation.setInformativeText(link.target);
  confirmation.setStandardButtons(QMessageBox::Open | QMessageBox::Cancel);
  confirmation.setDefaultButton(QMessageBox::Cancel);
  return confirmation.exec() == QMessageBox::Open;
}

} // namespace QindaQt::Apps::Terminal
