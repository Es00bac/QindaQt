// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <QString>
#include <optional>

namespace QindaQt::SessionSupervisor {

// One refresh-listed unit's running instance, as the user manager (MainPID)
// and /proc report it.
struct ResidentUnitInstance {
    QString unit;
    quint32 mainPid = 0;                // 0: the unit has no running process
    std::optional<quint64> startTicks;  // /proc/<pid>/stat starttime
    bool executableReplaced = false;    // /proc/<pid>/exe ends in " (deleted)"
};

enum class ResidentUnitState {
    NotRunning,
    Unobservable,
    StartedThisLogin,
    PredatesLogin,
    ExecutableReplaced,
};

// AGENT-CONTRACT (ADR-0379): the login refresh restarts a unit only when its
// running instance predates this login (started before the witnessed
// compositor, whose starttime is loginStartTicks) or runs a replaced
// executable. A unit started during this login keeps running: restarting
// xdg-desktop-portal 3.5 s after it started killed the screen-capture broker.
// A unit with no process is not started here; D-Bus activation starts it with
// the published environment. Unreadable facts never cause a restart.
[[nodiscard]] ResidentUnitState classifyResidentUnit(
    const ResidentUnitInstance &instance, std::optional<quint64> loginStartTicks);
[[nodiscard]] bool residentUnitIsStale(ResidentUnitState state);
[[nodiscard]] const char *residentUnitStateName(ResidentUnitState state);

// Field 22 of /proc/<pid>/stat (clock ticks after boot). Comparable across
// processes regardless of suspend time, unlike wall-clock start times.
[[nodiscard]] std::optional<quint64> processStartTicks(qint64 pid, const QString &procRoot = {});
// Reads the /proc/<pid>/exe link text without following it.
[[nodiscard]] bool processExecutableReplaced(qint64 pid, const QString &procRoot = {});

} // namespace QindaQt::SessionSupervisor
