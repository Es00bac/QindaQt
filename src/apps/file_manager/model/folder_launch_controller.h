// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "open_with_launcher.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace QindaQt::Apps::FileManager {

// AGENT-CONTRACT (ADR-0269): starts another program on one local folder --
// the desktop's terminal (Open Terminal Here: QQ_Term as
// `qqterm --working-directory <folder>`, the terminal the shell launcher
// already routes Terminal=true entries to), or another File Manager window
// (Open in New Window, through Desktop::FileBoundary::openLocalFolder, which
// also refuses a folder that is no longer the one listed). The folder is
// resolved once to a canonical, readable, enterable local directory and
// passed as one literal argv element: no shell, no URL handler, no
// default-application lookup. Success means the process started; its exit is
// not observed. GUI-thread only.
class FolderLaunchController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)

public:
  // The program lists are absolute candidates in trial order (production:
  // terminalProgramCandidates() and FileBoundary::fileManagerProgramCandidates()).
  FolderLaunchController(DetachedStarter start, QStringList terminalPrograms,
                         QStringList fileManagerPrograms, QObject *parent = nullptr);

  // Open Terminal Here, for the browsed folder.
  Q_INVOKABLE bool openTerminal(const QString &folderPath);
  // Open in New Window, for one listed folder's entry snapshot (its path and
  // the decimal device/inode strings the listing reported).
  Q_INVOKABLE bool openInNewWindow(const QVariantMap &entry);
  Q_INVOKABLE void clearLastError();

  [[nodiscard]] QString lastError() const { return m_lastError; }
  // QQ_Term's absolute path on PATH, when it is installed.
  [[nodiscard]] static QStringList terminalProgramCandidates();

signals:
  void lastErrorChanged();

private:
  void setLastError(const QString &message);

  DetachedStarter m_start;
  QStringList m_terminalPrograms;
  QStringList m_fileManagerPrograms;
  QString m_lastError;
};

} // namespace QindaQt::Apps::FileManager
