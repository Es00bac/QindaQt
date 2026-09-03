// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/terminal_widget_adapter.h"

#include "ui/terminal_appearance.h"

#include <qtermwidget.h>

#include <QDir>
#include <QFile>
#include <QFont>
#include <QStandardPaths>

#include <unistd.h>

namespace QindaQt::Apps::Terminal {

void TerminalWidgetAdapter::applyAppearance() {
  if (m_widget == nullptr) {
    return;
  }
  // AGENT-NOTE: The scheme is per-instance and installed with exclusive
  // temporary creation. A pre-planted symlink cannot be truncated, while a
  // crash-stale final path from PID reuse is removed before the atomic rename.
  const QString cacheDirectory =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  if (!cacheDirectory.isEmpty() && QDir().mkpath(cacheDirectory)) {
    static int instanceCounter = 0;
    // AGENT-CONTRACT (qtermwidget 2.4): a custom scheme path must end in
    // `.colorscheme`; another extension silently selects an upstream default.
    const QString baseName =
        QStringLiteral("qindaqt-terminal-scheme-%1-%2.colorscheme")
            .arg(::getpid())
            .arg(++instanceCounter);
    const QString targetPath = QDir(cacheDirectory).filePath(baseName);
    const QString temporaryPath = targetPath + QStringLiteral(".tmp");
    QFile schemeFile(temporaryPath);
    if (schemeFile.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
      const QByteArray document =
          TerminalColorSchemeDocument::render(m_appearance).toUtf8();
      if (schemeFile.write(document) == document.size() && schemeFile.flush()) {
        schemeFile.close();
        if (QFile::exists(targetPath)) {
          QFile::remove(targetPath);
        }
        if (QFile::rename(temporaryPath, targetPath)) {
          m_schemePath = targetPath;
          m_widget->setColorScheme(m_schemePath);
        }
      }
    }
    QFile::remove(temporaryPath);
  }

  QFont terminalFont = m_appearance.terminalFont;
  if (!m_profile.fontFamily.isEmpty()) {
    terminalFont.setFamily(m_profile.fontFamily);
  }
  if (m_profile.fontSize > 0) {
    terminalFont.setPointSize(m_profile.fontSize);
  }
  m_widget->setTerminalFont(terminalFont);
  m_widget->setHistorySize(m_profile.scrollbackLines);
}

} // namespace QindaQt::Apps::Terminal
