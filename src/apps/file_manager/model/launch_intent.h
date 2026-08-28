// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <memory>

namespace QindaQt::Apps::FileManager {

enum class LaunchError {
  None,
  NotFound,
  NotRegularFile,
  Unreadable,
  LaunchFailed,
};

struct LaunchResult final {
  LaunchError error = LaunchError::None;
  QString diagnostic;

  [[nodiscard]] bool ok() const { return error == LaunchError::None; }
};

// AGENT-CONTRACT: An implementation opens only an already-validated local
// regular file through the desktop's default handler for its type. It never
// executes an arbitrary command, interprets shell metacharacters, or follows
// to a non-regular final target. This is S0's complete file-opening contract;
// custom handler selection, portal-mediated launch, and destructive file
// operations remain later slices.
class FileLauncher {
public:
  virtual ~FileLauncher() = default;

  [[nodiscard]] virtual LaunchResult launch(const QString &absolutePath) const = 0;
};

using FileLauncherPtr = std::unique_ptr<FileLauncher>;

// Resolves an existing symlink once to its canonical target (mirroring the
// Text Editor's open contract) so opening never launches a dangling link, and
// validates the canonical target is a readable regular file before handing it
// to the desktop shell.
class DesktopFileLauncher final : public FileLauncher {
public:
  [[nodiscard]] LaunchResult launch(const QString &absolutePath) const override;

  // AGENT-NOTE: Exposed so tests can verify every pre-flight rejection
  // deterministically. A live QDesktopServices::openUrl() result depends on
  // the desktop's actual configured handlers, so it is not something a
  // hostile unit test can assert on for a successful case; only launch()
  // itself exercises that call.
  [[nodiscard]] static LaunchResult
  validateRegularFile(const QString &absolutePath, QString *canonicalPath);
};

} // namespace QindaQt::Apps::FileManager
