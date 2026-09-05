// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_controls/places_controller.h"

#include <QStringList>

namespace QindaQt::Shell::Launcher {
class LaunchSpawner;
}

namespace QindaQt::Shell::DesktopControls {

// Production FolderOpener: starts the first-party file manager with one
// absolute directory argument through the launcher's bounded process seam.
//
// AGENT-CONTRACT: `programCandidates` are absolute paths tried in order; the
// first regular executable file wins. Relative names are never searched here
// so an attacker-controlled PATH cannot select the program; the composition
// root supplies the shell's sibling binary and the configured install prefix.
// No shell is ever involved (ADR-0062).
class FileManagerFolderOpener final : public FolderOpener {
public:
  static QStringList defaultProgramCandidates();

  FileManagerFolderOpener(Launcher::LaunchSpawner &spawner,
                          QStringList programCandidates);

  [[nodiscard]] Result open(const QString &absoluteDirectory) override;
  [[nodiscard]] QString resolvedProgram() const;

private:
  Launcher::LaunchSpawner &m_spawner;
  QStringList m_programCandidates;
};

} // namespace QindaQt::Shell::DesktopControls
