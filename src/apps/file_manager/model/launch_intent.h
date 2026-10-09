// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <functional>
#include <memory>

namespace QindaQt::WindowsRunner {
class WindowsExecutableOpenPort;
struct WindowsExecutableOpenResult;
}

namespace QindaQt::Apps::FileManager {

enum class LaunchError {
  None,
  NotFound,
  NotRegularFile,
  Unreadable,
  LaunchFailed,
};

enum class LaunchProgress { Complete, Starting, SetupOpened, OutcomeUnknown };

struct LaunchResult final {
  LaunchError error = LaunchError::None;
  QString diagnostic;
  LaunchProgress progress = LaunchProgress::Complete;

  [[nodiscard]] bool ok() const { return error == LaunchError::None; }
};

// AGENT-CONTRACT: open one already-validated local regular file through the
// current chosen handler. The optional public Windows route additionally
// admits actual PE/file identity and owns first-run/profile setup; no extension,
// shell string or game metadata becomes authority. Submission is bounded and
// later process/window success is reported only by its owning runtime.
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
  // The optional borrowed Windows port outlives this launcher. Its current
  // default/profile policy can route a selected PE into visible setup/startup;
  // a deliberate native user default retains ordinary desktop dispatch.
  using DefaultHandler = std::function<bool(const QString &canonicalPath)>;
  explicit DesktopFileLauncher(
      QindaQt::WindowsRunner::WindowsExecutableOpenPort *windows = nullptr,
      DefaultHandler defaultHandler = {});
  [[nodiscard]] LaunchResult launch(const QString &absolutePath) const override;

  // AGENT-NOTE: Exposed so tests can verify every pre-flight rejection
  // deterministically. A live QDesktopServices::openUrl() result depends on
  // the desktop's actual configured handlers, so it is not something a
  // hostile unit test can assert on for a successful case; only launch()
  // itself exercises that call.
  [[nodiscard]] static LaunchResult
  validateRegularFile(const QString &absolutePath, QString *canonicalPath);
private:
  QindaQt::WindowsRunner::WindowsExecutableOpenPort *m_windows;
  DefaultHandler m_defaultHandler;
};

[[nodiscard]] LaunchResult windowsExecutableLaunchResult(
    const QindaQt::WindowsRunner::WindowsExecutableOpenResult &result);

} // namespace QindaQt::Apps::FileManager
