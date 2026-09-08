// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include "ui/terminal_widget_adapter.h"

#include "ui/terminal_appearance.h"

#include <qtermwidget.h>

#include <QDir>
#include <QFile>
#include <QFont>
#include <QStandardPaths>

#include <unistd.h>

namespace QindaQt::Apps::Terminal {

void TerminalWidgetAdapter::setAppearance(const TerminalViewAppearance &appearance) {
  m_appearance = appearance;
  // AGENT-GUARD: A saved profile's explicit scheme survives desktop refreshes.
  // High contrast temporarily takes precedence; retain the original palette so
  // disabling it restores the user's choice without restarting their shell.
  if (m_profile.id != builtinDefaultProfileId() && !appearance.highContrast) {
    m_appearance.terminalBackground = m_profileAppearance.terminalBackground;
    m_appearance.terminalForeground = m_profileAppearance.terminalForeground;
    for (int index = 0; index < 16; ++index)
      m_appearance.ansi[index] = m_profileAppearance.ansi[index];
  }
  applyAppearance();
}

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
          const QString previousPath = m_schemePath;
          m_schemePath = targetPath;
          m_widget->setColorScheme(m_schemePath);
          if (!previousPath.isEmpty())
            QFile::remove(previousPath);
        }
      }
    }
    QFile::remove(temporaryPath);
  }

  applyFont();
  m_widget->setHistorySize(m_profile.scrollbackLines);
}

void TerminalWidgetAdapter::applyFont() {
  if (!m_widget) return;
  QFont terminalFont = m_appearance.terminalFont;
  if (!m_profile.fontFamily.isEmpty()) {
    terminalFont.setFamily(m_profile.fontFamily);
  }
  if (m_profile.fontSize > 0) {
    terminalFont.setPointSizeF(m_profile.fontSize * m_appearance.textScale);
  }
  terminalFont.setPointSizeF(std::clamp(terminalFont.pointSizeF() + m_zoomSteps, 6.0, 48.0));
  m_widget->setTerminalFont(terminalFont);
}

} // namespace QindaQt::Apps::Terminal
