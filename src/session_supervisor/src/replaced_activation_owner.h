// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "session_activation_policy.h"
#include <QStringList>
#include <QtDBus/QDBusConnection>

namespace QindaQt::SessionSupervisor {

// AGENT-NOTE: D-Bus-activated QindaQt services that keep install-versioned
// state (Settings1's schema, the portal's Settings1 projection) live on the
// persistent user bus, so logging out never stops them. After a package
// update the old process kept answering with the old schema and rejected
// every snapshot carrying new keys ("Settings snapshot is malformed or
// regressed", 2026-09-28). Unlike resident_service_refresh.h this never
// restarts a healthy owner: it retires only an owner whose executable has
// been replaced since it started, and D-Bus activation starts the installed
// one on the next call.
[[nodiscard]] QStringList replacedActivationServiceNames();

// Pure: a /proc/<pid>/exe link target of a process whose file was replaced
// or removed ends in " (deleted)".
[[nodiscard]] bool executableWasReplaced(const QString &exeLinkTarget);

// For each well-known name, finds its owner's pid through the bus, and when
// that process's executable was replaced (read from `procRoot`, "/proc"
// unless a test injects one) sends SIGTERM and waits up to two seconds for
// the name to be released. Returns the names retired. Best effort: an
// unreadable owner, another user's process or a timeout is logged and
// skipped. AGENT-GUARD: Private scope never signals anything (the shared
// broker belongs to the physical desktop; see resident_service_refresh.h).
QStringList retireReplacedActivationOwners(const QDBusConnection &bus,
                                           const QStringList &serviceNames,
                                           SessionActivationScope scope,
                                           const QString &procRoot = {});

} // namespace QindaQt::SessionSupervisor
