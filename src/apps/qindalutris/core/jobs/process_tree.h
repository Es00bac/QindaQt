// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

class QTimer;

namespace QindaQt::QindaLutris {

// AGENT-CONTRACT: stopping a WHOLE process tree (ADR-0275 section 4b, "Force
// quit"). umu-run starts the Steam Runtime with start_new_session=True and
// only forwards SIGTERM/SIGINT, so killing the direct child leaves
// pressure-vessel, Wine and the vendor installer running. The authority is a
// transient systemd user scope per job/launch:
//   systemd-run --user --scope --quiet --collect --unit=<name> -- <argv>
// and stopping is `systemctl --user kill --signal=SIGTERM <name>.scope`,
// a bounded grace, then SIGKILL, and "stopped" only once the scope and every
// tracked process are gone. Without a user manager the fallback is the
// child's own process group plus /proc descendant tracking (pid + start
// time); a daemon that double-forked away before it was tracked can escape
// the fallback -- the scope has no such hole.
// Shared by the install jobs' ProcessRunner (process_runner.h) and, next,
// the game launcher's Force quit: both must use these helpers so there is
// one naming scheme and one kill sequence. Linux only; threading: owner
// thread for ProcessTreeStopper, the free functions are thread-safe.

// One process, identified by pid AND kernel start time, so a recycled pid is
// never signalled by mistake.
struct ProcessIdentity final {
  qint64 pid = 0;
  quint64 startTime = 0;

  friend bool operator==(const ProcessIdentity &, const ProcessIdentity &) = default;
};

// nullopt when the pid does not exist or is a zombie.
[[nodiscard]] std::optional<ProcessIdentity> liveProcessIdentity(qint64 pid);
// True while that exact process (same start time) exists and is not a zombie.
[[nodiscard]] bool isProcessAlive(const ProcessIdentity &process);
// Every live descendant of the given roots (bounded /proc walk).
[[nodiscard]] QVector<ProcessIdentity> liveDescendants(const QVector<qint64> &rootPids,
                                                       int maxProcesses = 32768);
// Signals the process only if it is still that same process.
void signalProcess(const ProcessIdentity &process, int signalNumber);

// "<prefix>-<32 hex>"; a valid systemd unit name stem (append ".scope").
[[nodiscard]] QString newScopeUnitName(const QString &prefix = QStringLiteral("qindalutris-job"));
// Arguments for `systemd-run` that run program+arguments in scope `unit`.
// The scope inherits systemd-run's environment, so QProcess overlays still
// reach the program (systemd-run --scope execs it in place).
[[nodiscard]] QStringList systemdRunScopeArguments(const QString &unit, const QString &program,
                                                   const QStringList &arguments);
// Arguments for `systemctl`: --user kill --signal=<name> <unit>.scope
[[nodiscard]] QStringList systemctlKillArguments(const QString &unit, const QString &signalName);

struct UserScopeTools final {
  QString systemdRun;
  QString systemctl;
  [[nodiscard]] bool available() const { return !systemdRun.isEmpty() && !systemctl.isEmpty(); }
};
// Resolves systemd-run/systemctl and confirms `systemctl --user
// is-system-running` answers running/degraded/starting (bounded, ~2 s max).
// Empty tools when there is no usable user manager.
[[nodiscard]] UserScopeTools detectUserScopeTools();
// True while `systemctl --user is-active <unit>.scope` reports it active.
[[nodiscard]] bool isScopeActive(const UserScopeTools &tools, const QString &unit);

struct StopTarget final {
  qint64 mainPid = 0;            // also the process-group id (setpgid at start)
  QString scopeUnit;             // empty in the fallback
  UserScopeTools tools;
  // Tracked while the tree ran; seed it with the main process's identity
  // (liveProcessIdentity(mainPid)) right after it starts.
  QVector<ProcessIdentity> known;
};

// Drops dead entries from target.known and adds every live descendant of
// the remaining ones. Runners call it periodically while the tree
// runs, so children that later leave the tree (double fork, setsid after
// the parent exits) are still tracked when a stop begins.
void trackProcessTree(StopTarget &target);

// Runs the kill sequence without blocking the event loop.
class ProcessTreeStopper final : public QObject {
  Q_OBJECT
public:
  explicit ProcessTreeStopper(QObject *parent = nullptr);

  void stop(const StopTarget &target, int graceMs = 5000, int killWaitMs = 3000);
  [[nodiscard]] bool isStopping() const { return m_phase != Phase::Idle; }

  // The same sequence, blocking (destructors only). Returns treeGone.
  static bool stopBlocking(const StopTarget &target, int graceMs = 5000, int killWaitMs = 3000);

Q_SIGNALS:
  // treeGone is false when something was still alive after SIGKILL + wait.
  void stopped(bool treeGone, const QString &detail);

private:
  enum class Phase { Idle, Terminating, Killing };
  void poll();

  StopTarget m_target;
  Phase m_phase = Phase::Idle;
  QTimer *m_timer = nullptr;
  qint64 m_deadline = 0;
  int m_killWaitMs = 0;
  qsizetype m_signalled = 0;
};

} // namespace QindaQt::QindaLutris
