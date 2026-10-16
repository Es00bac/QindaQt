// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <systemd/sd-bus.h>
#include <array>
#include <optional>
#include <string>
namespace qindaqt::keyring::pambridge {
// AGENT-CONTRACT: the bridge asks the root system manager four questions. Each
// is a method of interface org.freedesktop.systemd1.Manager on object
// /org/freedesktop/systemd1 owned by bus name org.freedesktop.systemd1. The
// system bus policy (org.freedesktop.systemd1.conf) admits an unprivileged
// caller per (destination, interface, member): GetUnit, GetUnitByPIDFD and
// LoadUnit are open to everyone and StartUnit is bus-allowed then polkit-gated.
// The bus NAME is not the interface. Passing "org.freedesktop.systemd1" as the
// interface matches no allow rule, the default deny rejects the call
// ("Rejected send message ... interface=org.freedesktop.systemd1
// member=GetUnit" in the system journal) and lock-screen keyring unlock failed
// silently on every unlock (stability round 2026-10-16, finding 23). The wire
// message is pinned by tests/services/keyring/pam/test_manager_calls.cpp.
struct ManagerCall {
    const char *destination;
    const char *path;
    const char *interface;
    const char *member;
};
inline constexpr char SystemdBusName[] = "org.freedesktop.systemd1";
inline constexpr char SystemdManagerPath[] = "/org/freedesktop/systemd1";
inline constexpr char SystemdManagerInterface[] = "org.freedesktop.systemd1.Manager";
inline constexpr ManagerCall GetUnitCall{SystemdBusName, SystemdManagerPath, SystemdManagerInterface, "GetUnit"};
inline constexpr ManagerCall LoadUnitCall{SystemdBusName, SystemdManagerPath, SystemdManagerInterface, "LoadUnit"};
inline constexpr ManagerCall StartUnitCall{SystemdBusName, SystemdManagerPath, SystemdManagerInterface, "StartUnit"};
inline constexpr ManagerCall GetUnitByPidfdCall{SystemdBusName, SystemdManagerPath, SystemdManagerInterface,
                                                "GetUnitByPIDFD"};

// One synchronous manager call with sd_bus_call_method argument conventions.
// Returns the sd-bus status (negative errno on failure); on success a non-null
// `reply` receives a message the caller must unref.
int callManager(sd_bus *bus, const ManagerCall &call, sd_bus_message **reply, const char *types, ...);

// LoadUnit when `privileged` (root may load an inactive unit) otherwise GetUnit
// (only an already loaded unit). Returns the unit's object path, or empty.
std::string managerUnitPath(sd_bus *bus, const std::string &unit, bool privileged);
// StartUnit(unit, "fail"): never replaces a queued job.
bool managerStartUnit(sd_bus *bus, const std::string &unit);

// GetUnitByPIDFD: the unit that owns the process behind a held pidfd.
struct PidfdUnit {
    std::string path, id;
    std::array<unsigned char, 16> invocation{};
};
std::optional<PidfdUnit> managerUnitByPidfd(sd_bus *bus, int pidfd);
}
