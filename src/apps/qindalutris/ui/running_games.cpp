// SPDX-License-Identifier: GPL-3.0-or-later
#include "running_games.h"

#include "library_controller.h"
#include "process_supervisor.h"
#include "scoped_game_launcher.h"

#include <QProcess>
#include <QTimer>

namespace QindaQt::QindaLutris {

namespace {

constexpr int kPollIntervalMs = 3000;
constexpr int kStopGraceMs = 5000;
constexpr int kStopKillWaitMs = 3000;

} // namespace

RunningGames::RunningGames(LibraryController *library, ScopedGameLauncher *launcher,
                           QObject *parent)
    : QObject(parent), m_library(library), m_launcher(launcher),
      m_pollTimer(new QTimer(this)) {
  connect(m_library, &LibraryController::gameLaunched, this, &RunningGames::onLaunched);
  m_pollTimer->setInterval(kPollIntervalMs);
  connect(m_pollTimer, &QTimer::timeout, this, &RunningGames::poll);
}

// Supervisors are children: their destructors stop an in-flight Force quit
// (blocking, bounded) rather than leave a half-stopped game behind.
RunningGames::~RunningGames() = default;

QStringList RunningGames::activeUnits(const QStringList &units, const QByteArray &output) {
  const QList<QByteArray> lines = output.split('\n');
  QStringList active;
  for (qsizetype i = 0; i < units.size() && i < lines.size(); ++i) {
    const QByteArray state = lines.at(i).trimmed();
    if (state == "active" || state == "activating" || state == "deactivating") {
      active.append(units.at(i));
    }
  }
  return active;
}

void RunningGames::onLaunched(const QString &gameId) {
  const QString unit = m_launcher->lastScopeUnit();
  if (unit.isEmpty()) {
    return;
  }
  m_units.insert(gameId, unit);
  m_message.clear();
  if (!m_pollTimer->isActive()) {
    m_pollTimer->start();
  }
  Q_EMIT changed();
}

void RunningGames::poll() {
  if (m_poll != nullptr || m_units.isEmpty()) {
    if (m_units.isEmpty()) {
      m_pollTimer->stop();
    }
    return;
  }
  const QStringList ids = m_units.keys();
  QStringList units;
  QStringList unitFiles; // systemd-run --unit=X creates X.scope
  for (const QString &id : ids) {
    units.append(m_units.value(id));
    unitFiles.append(m_units.value(id) + QStringLiteral(".scope"));
  }
  auto *process = new QProcess(this);
  m_poll = process;
  const auto release = [this, process] {
    process->deleteLater();
    if (m_poll == process) {
      m_poll = nullptr;
    }
  };
  connect(process, &QProcess::finished, this, [this, process, release, ids, units] {
    const QStringList active = activeUnits(units, process->readAllStandardOutput());
    // activeUnits pairs states with `units` by position; unitFiles is the
    // same list with the .scope suffix systemctl needs.
    release();
    bool changedAny = false;
    for (qsizetype i = 0; i < ids.size(); ++i) {
      if (!active.contains(units.at(i)) && !m_stopping.contains(ids.at(i)) &&
          m_units.value(ids.at(i)) == units.at(i)) {
        m_units.remove(ids.at(i));
        changedAny = true;
      }
    }
    if (changedAny) {
      Q_EMIT changed();
    }
  });
  connect(process, &QProcess::errorOccurred, this, [release](QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
      release(); // finished() never follows a failed start
    }
  });
  process->start(m_launcher->tools().systemctl,
                QStringList{QStringLiteral("--user"), QStringLiteral("is-active")} + unitFiles);
}

void RunningGames::forceQuit(const QString &gameId) {
  const QString unit = m_units.value(gameId);
  if (unit.isEmpty() || m_stopping.contains(gameId)) {
    return;
  }
  auto *supervisor = new ProcessTreeSupervisor(this);
  m_stopping.insert(gameId, supervisor);
  connect(supervisor, &ProcessTreeSupervisor::settled, this,
          [this, gameId](const TreeStopOutcome &outcome) { onStopped(gameId, outcome); });
  StopTarget target;
  target.scopeUnit = unit;
  target.tools = m_launcher->tools();
  supervisor->watch(target, kStopGraceMs, kStopKillWaitMs);
  supervisor->requestStop();
  m_message = QStringLiteral("Stopping the game…");
  Q_EMIT changed();
}

void RunningGames::onStopped(const QString &gameId, const TreeStopOutcome &outcome) {
  if (auto *supervisor = m_stopping.take(gameId)) {
    supervisor->deleteLater();
  }
  if (outcome.proven) {
    m_units.remove(gameId);
    m_message = QStringLiteral("The game was stopped.");
  } else {
    m_message = QStringLiteral("The game could not be stopped completely. Log out and back "
                               "in to end what is left.");
  }
  Q_EMIT changed();
}

} // namespace QindaQt::QindaLutris
