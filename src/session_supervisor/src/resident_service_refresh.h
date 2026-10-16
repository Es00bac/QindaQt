// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "session_activation_policy.h"
#include <QStringList>
#include <QtDBus/QDBusConnection>

#include <functional>
#include <optional>

namespace QindaQt::SessionSupervisor {
// AGENT-GUARD: Private scope (the default) never mutates the shared broker or
// user manager without an explicit hermetic endpoint. Only the witnessed
// physical compositor may use the native host manager. See compositor-session
// wiki, "Private desktop isolation". Private harnesses own broker setup.


// This fixed list contains services that cache desktop session state across
// logins, plus the narrow Audio1 package-upgrade exception: a persistent user
// manager can retain its old D-Bus ABI after the installed binary changes.
// AGENT-NOTE: source of truth is each service's owning module.
// `xdg-desktop-portal` (the portal frontend) selects and caches which backend
// it routes to, from `XDG_CURRENT_DESKTOP`, once at its own startup rather
// than per call (ADR-0094). The other two hold their own direct Wayland
// connection: `qindaqt-clipboard-host` (ADR-0058, wl_display_connect in
// clipboard_wayland_adapter) and `qindaqt-display-service` (ADR-0053,
// wl_display_connect in display_writer's output-management port). The retired
// Plasma portal backend is no longer listed (ADR-0379). Ordinary D-Bus-only
// services remain excluded unless a specific lifecycle defect is
// demonstrated, as it is for Audio1's upgrade-stale process.
[[nodiscard]] QStringList residentServiceRefreshUnits();

// Login reference for the staleness rule in resident_unit_staleness.h: the
// witnessed compositor's /proc starttime. Without it only replaced executables
// are restarted. `procRoot` is a hermetic test seam ("/proc" when empty).
struct ResidentRefreshContext {
    std::optional<quint64> loginStartTicks;
    QString procRoot;
};

// For each named systemd user unit, in the order given, asks the user manager
// for its MainPID and restarts it with `RestartUnit(name, "replace")` only when
// its running instance is stale (resident_unit_staleness.h): it predates this
// login or runs a replaced executable. A unit started during this login, or
// not running at all, is left alone; D-Bus activation starts a missing one
// with the published environment. The manager is reached through its private
// control socket when it exists, otherwise through the supplied session bus
// when the manager actually owns org.freedesktop.systemd1 there.
// `systemdPrivateSocketPath` overrides the socket location for hermetic tests.
// Call after publishActivationEnvironment and before starting desktop
// consumers. AGENT-CONTRACT: a unit already resident from a prior desktop
// keeps its stale Wayland connection or cached routing until the process
// itself restarts; SetEnvironment only changes the environment future
// activations receive. `RestartUnit` enqueues a systemd job and returns a job
// object path; it does not wait for the restart to finish. For a restarted
// Audio1, this function waits up to two seconds for its old unique owner
// to disappear. It returns false if that owner cannot be proven retired;
// the helper logs this failure and keeps the desktop available. Other unit
// restart failures remain best-effort, and no unit outside the fixed list is
// touched.
bool refreshResidentServices(const QDBusConnection &bus,
                             const QStringList &unitNames,
                             const QString &systemdPrivateSocketPath = {},
                             SessionActivationScope scope = SessionActivationScope::Private,
                             const ResidentRefreshContext &context = {});

// AGENT-NOTE (2026-09-28): a PipeWire daemon that stopped answering new
// clients survived two logins in the persistent user manager; every later
// recording captured nothing and dictation recognized nothing. These are the
// units of the audio stack, in restart order.
[[nodiscard]] QStringList pipeWireStackUnits();

// One bounded client round trip (`pw-cli info 0`). True when PipeWire answered,
// and also when there is no pw-cli to ask: the stack is never restarted blindly.
[[nodiscard]] bool pipeWireAnswers(int timeoutMilliseconds = 3'000);

// Restarts pipeWireStackUnits() on the user manager when `answers` reports a
// daemon that does not answer; returns true only when a restart was requested.
// Route and scope rules are refreshResidentServices'; Private scope never
// touches the shared manager without an explicit socket. AGENT-CONTRACT:
// qindaqt-session calls this before refreshResidentServices, so Audio1 and
// every recorder reconnect to a daemon that answers.
bool refreshUnresponsivePipeWire(const QDBusConnection &bus,
                                 const std::function<bool()> &answers,
                                 const QString &systemdPrivateSocketPath = {},
                                 SessionActivationScope scope = SessionActivationScope::Private);

} // namespace QindaQt::SessionSupervisor
