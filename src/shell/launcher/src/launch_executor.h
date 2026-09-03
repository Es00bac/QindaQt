// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "launch_activator.h"
#include "launch_spawner.h"

#include <QObject>
#include <QString>
#include <QStringList>

namespace QindaQt::Shell::Launcher {

class ApplicationScanner;

enum class LaunchStatus {
  Spawned,             // process start accepted by the spawner
  ActivationRequested, // D-Bus activation dispatched; completion follows
  Refused,             // policy/shape refusal before any side effect
  Failed,              // the spawner or bus reported failure
};

struct LaunchOutcome {
  LaunchStatus status = LaunchStatus::Refused;
  QString diagnostic;

  bool ok() const
  {
    return status == LaunchStatus::Spawned
        || status == LaunchStatus::ActivationRequested;
  }

  friend bool operator==(const LaunchOutcome &, const LaunchOutcome &) = default;
};

// Turns an L0 launch intent into a bounded execution action. Every launch
// resolves through the catalog's single intent builder (ADR-0042), then
// extracts execution keys from the scanner-retained document, plans argv
// without a shell, and dispatches through injected seams:
//
// - DBusActivatable=true  -> LaunchActivator (org.freedesktop.Application)
// - Terminal=true         -> the injected terminal command prefix, or a
//                            truthful refusal when no terminal policy is wired
// - otherwise             -> LaunchSpawner with the entry's Path
//
// AGENT-CONTRACT: The borrowed scanner, spawner, and activator must outlive
// this executor. The executor never retries and never constructs its own
// seams; activation tokens are a later slice (ADR-0056).
class LaunchExecutor final : public QObject {
  Q_OBJECT
public:
  LaunchExecutor(const ApplicationScanner &scanner, LaunchSpawner &spawner,
                 LaunchActivator &activator, QStringList terminalCommand = {},
                 QObject *parent = nullptr);

  [[nodiscard]] const LaunchOutcome &lastOutcome() const noexcept
  {
    return m_lastOutcome;
  }

  // Empty actionId launches the entry's primary action.
  LaunchOutcome launch(const QString &entryId, const QString &actionId = {});

Q_SIGNALS:
  void launchFinished(const QindaQt::Shell::Launcher::LaunchOutcome &outcome);
  void activationFinished(const QString &desktopId, bool ok,
                          const QString &diagnostic);

private:
  const ApplicationScanner &m_scanner;
  LaunchSpawner &m_spawner;
  LaunchActivator &m_activator;
  QStringList m_terminalCommand;
  LaunchOutcome m_lastOutcome;
};

} // namespace QindaQt::Shell::Launcher
