// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "session_activation_policy.h"
#include <QStringList>
#include <QtDBus/QDBusConnection>
#include <functional>
#include <memory>
#include <optional>

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

// Private owning test seam: same-thread witness pins one process lifetime.
// Production supplies a pidfd implementation; failed/changed identity never
// grants a signal. The factory returns an owning witness, not a numeric PID
// signal capability. Tests may inject one only for their own broker names.
struct ReplacedOwnerIdentity {
    quint32 userId = 0;
    quint64 startTime = 0;
    QString executable;
    bool operator==(const ReplacedOwnerIdentity &) const = default;
};
class ReplacedOwnerProcessWitness {
public:
    virtual ~ReplacedOwnerProcessWitness() = default;
    virtual std::optional<ReplacedOwnerIdentity> identity() const = 0;
    virtual bool terminate() = 0;
};
using ReplacedOwnerWitnessFactory =
    std::function<std::unique_ptr<ReplacedOwnerProcessWitness>(qint64, const QString &)>;

// Pure: a /proc/<pid>/exe link target of a process whose file was replaced
// or removed ends in " (deleted)".
[[nodiscard]] bool executableWasReplaced(const QString &exeLinkTarget);

// For each well-known name, finds its owner's pid through the bus, and when
// that process's executable was replaced (read from `procRoot`, "/proc"
// unless a test injects one) pins its same-user lifetime, rechecks current owner/PID/identity, sends
// SIGTERM through the held pidfd and waits up to two seconds for
// the name to be released. Returns the names retired. Best effort: an
// unreadable owner, another user's process or a timeout is logged and
// skipped. AGENT-GUARD: Private scope never signals anything, even with a test procRoot (the shared
// broker belongs to the physical desktop; see resident_service_refresh.h).
QStringList retireReplacedActivationOwners(const QDBusConnection &bus,
                                           const QStringList &serviceNames,
                                           SessionActivationScope scope,
                                           const QString &procRoot = {},
                                           const ReplacedOwnerWitnessFactory &witnessFactory = {});

} // namespace QindaQt::SessionSupervisor
