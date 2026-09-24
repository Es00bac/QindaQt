// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "launch_intent.h"

#include "qindaqt/application_catalog/application_directory_scan.h"

#include <QString>
#include <QStringList>

#include <functional>

namespace QindaQt::Apps::FileManager {

// Starts `program` detached with `arguments` as literal argv elements.
using DetachedStarter =
    std::function<bool(const QString &program, const QStringList &arguments)>;

// AGENT-CONTRACT (ADR-0269, widening ADR-0029's bounded launch): opens local
// regular files with one application the user named. Each path is validated
// exactly as DesktopFileLauncher::validateRegularFile validates a document
// (it exists, a link is resolved once, the target is a readable regular file)
// and is handed over canonical. The application's own scan-retained Exec line
// receives the paths through the shared launcher grammar's %f/%F/%u/%U codes
// as whole argv elements; an Exec that takes one file (%f/%u) is started once
// per file, and one that takes none is refused rather than started without
// the files. Nothing is interpreted by a shell and nothing is guessed from a
// URL or a MIME type: the caller names the application. A Terminal=true entry
// runs inside the desktop's terminal prefix (QQ_Term's `qqterm -e`, the shell
// launcher's own policy); a D-Bus-activatable or unplannable entry is refused
// with a typed LaunchFailed. Success means every planned process started;
// their later exits are not observed. Stateless apart from its two seams.
class OpenWithLauncher final {
public:
  // At most this many files per request, bounding the process count and
  // leaving room under the launcher grammar's 64-argument ceiling.
  static constexpr int maximumFiles = 32;

  OpenWithLauncher(DetachedStarter start, QStringList terminalCommandPrefix);

  [[nodiscard]] LaunchResult launch(
      const QindaQt::ApplicationCatalog::ScannedApplication &application,
      const QStringList &absolutePaths) const;

  // Production seams: QProcess::startDetached, and QQ_Term's `qqterm -e`.
  [[nodiscard]] static DetachedStarter processStarter();
  [[nodiscard]] static QStringList desktopTerminalPrefix();

private:
  DetachedStarter m_start;
  QStringList m_terminalPrefix;
};

} // namespace QindaQt::Apps::FileManager
