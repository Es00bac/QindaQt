// SPDX-License-Identifier: LGPL-3.0-or-later
#include "launch_executor.h"

#include "application_scanner.h"
#include "launch_execution.h"

namespace QindaQt::Shell::Launcher {

LaunchExecutor::LaunchExecutor(const ApplicationScanner &scanner,
                               LaunchSpawner &spawner,
                               LaunchActivator &activator,
                               QStringList terminalCommand, QObject *parent)
    : QObject(parent)
    , m_scanner(scanner)
    , m_spawner(spawner)
    , m_activator(activator)
    , m_terminalCommand(std::move(terminalCommand))
{
  connect(&m_activator, &LaunchActivator::activationFinished,
          this, &LaunchExecutor::activationFinished);
}

LaunchOutcome LaunchExecutor::launch(const QString &entryId,
                                     const QString &actionId)
{
  const auto refuse = [this](QString diagnostic) {
    m_lastOutcome = LaunchOutcome { LaunchStatus::Refused, std::move(diagnostic) };
    Q_EMIT launchFinished(m_lastOutcome);
    return m_lastOutcome;
  };

  const auto &catalog = m_scanner.catalog();
  if (!catalog) {
    return refuse(QStringLiteral("launcher catalog is not available"));
  }

  // AGENT-GUARD: The catalog's intent builder is the only activation
  // resolver; a launch for an entry the catalog does not publish (unknown,
  // hidden, or shadowed) must fail here, before any execution planning.
  const auto intent = catalog->makeLaunchIntent(entryId, actionId);
  if (!intent.ok()) {
    return refuse(QStringLiteral("entry or action is not launchable"));
  }

  const auto document = m_scanner.documentText(entryId);
  if (!document) {
    return refuse(QStringLiteral("entry document is not retained"));
  }

  // Entry-level execution keys decide the dispatch surface: a
  // DBusActivatable entry activates over D-Bus for every action, while the
  // action group's own Exec/Path/Terminal apply only when spawning.
  const auto entryParsed = LaunchExecutionParser::parse(*document);
  if (!entryParsed.ok()) {
    return refuse(QStringLiteral("entry execution data is unusable: %1")
                      .arg(entryParsed.message));
  }

  if (entryParsed.keys->dbusActivatable) {
    const ActivationDispatch dispatch = m_activator.activate(entryId, actionId);
    if (!dispatch.accepted) {
      m_lastOutcome = LaunchOutcome { LaunchStatus::Failed, dispatch.diagnostic };
    } else {
      m_lastOutcome = LaunchOutcome { LaunchStatus::ActivationRequested, {} };
    }
    Q_EMIT launchFinished(m_lastOutcome);
    return m_lastOutcome;
  }

  const auto parsed = actionId.isEmpty()
      ? entryParsed
      : LaunchExecutionParser::parse(*document, actionId);
  if (!parsed.ok()) {
    return refuse(QStringLiteral("entry execution data is unusable: %1")
                      .arg(parsed.message));
  }
  const ExecutionKeys &keys = *parsed.keys;

  const auto planned = ExecFieldCodeExpander::expand(
      keys.exec,
      ExecExpansionValues { intent.intent->displayName, intent.intent->iconName,
                            m_scanner.documentPath(entryId) });
  if (!planned.ok()) {
    return refuse(QStringLiteral("entry command is not launchable: %1")
                      .arg(planned.message));
  }

  if (keys.terminal) {
    if (m_terminalCommand.isEmpty()) {
      // AGENT-NOTE: Terminal=true without a wired terminal policy is a
      // truthful refusal, never a silent fallback to a shell. Wiring the
      // QindaQt Terminal launch policy is the composition root's job.
      return refuse(QStringLiteral("terminal launch policy is unavailable"));
    }
    SpawnRequest request;
    request.program = m_terminalCommand.constFirst();
    request.arguments = m_terminalCommand.mid(1)
        + QStringList { planned.plan->program } + planned.plan->arguments;
    request.workingDirectory = keys.path;
    const SpawnResult result = m_spawner.spawn(request);
    m_lastOutcome = result.ok
        ? LaunchOutcome { LaunchStatus::Spawned, {} }
        : LaunchOutcome { LaunchStatus::Failed, result.diagnostic };
    Q_EMIT launchFinished(m_lastOutcome);
    return m_lastOutcome;
  }

  SpawnRequest request;
  request.program = planned.plan->program;
  request.arguments = planned.plan->arguments;
  request.workingDirectory = keys.path;
  const SpawnResult result = m_spawner.spawn(request);
  m_lastOutcome = result.ok
      ? LaunchOutcome { LaunchStatus::Spawned, {} }
      : LaunchOutcome { LaunchStatus::Failed, result.diagnostic };
  Q_EMIT launchFinished(m_lastOutcome);
  return m_lastOutcome;
}

} // namespace QindaQt::Shell::Launcher
