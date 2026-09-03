// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

namespace QindaQt::Shell::Launcher {

// One fully planned process start. The program is always an argv[0], never a
// shell command line; no shell is involved anywhere in this boundary.
struct SpawnRequest {
  QString program;
  QStringList arguments;
  QString workingDirectory; // empty means inherit (entry had no Path key)

  friend bool operator==(const SpawnRequest &, const SpawnRequest &) = default;
};

struct SpawnResult {
  bool ok = false;
  QString diagnostic;

  friend bool operator==(const SpawnResult &, const SpawnResult &) = default;
};

// Process-start seam. Tests inject a recording fake; production uses
// QProcessLaunchSpawner. Implementations must be thread-confined to their
// owner's thread and must never invoke a shell.
class LaunchSpawner {
public:
  virtual ~LaunchSpawner() = default;
  virtual SpawnResult spawn(const SpawnRequest &request) = 0;
};

// The environment allowlist applied to launched applications. Anything not
// listed — including caller-injected session state such as QindaQt's own
// development overrides — does not propagate to child processes. LC_* is
// forwarded by prefix so locale variants survive.
QProcessEnvironment sanitizedChildEnvironment(const QProcessEnvironment &base);

// Production spawner: QProcess::startDetached with the sanitized environment
// and the entry's working directory. AGENT-CONTRACT: this is the only class
// in the launcher that may start a process; tests must use the seam with a
// fake or a fixture executable (see ADR-0056). Environment sanitization is
// tested through the free function above, so this class needs no test hook.
class QProcessLaunchSpawner final : public LaunchSpawner {
public:
  SpawnResult spawn(const SpawnRequest &request) override;
};

} // namespace QindaQt::Shell::Launcher
