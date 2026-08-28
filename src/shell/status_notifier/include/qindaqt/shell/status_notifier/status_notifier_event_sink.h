// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/shell/status_notifier/status_notifier_types.h>

#include <QtCore/QString>

#include <QtGlobal>

namespace QindaQt::StatusNotifier
{

// AGENT-CONTRACT: The narrow event vocabulary a transport adapter may drive.
// This is the entire authority a tray transport receives: it may report owner
// arrivals/departures, item registrations/removals, and watcher population
// epochs, and nothing else. Observation, request evaluation, and degradation
// acknowledgement are registry-only surfaces that no transport can reach
// through this interface.
//
// Lifetime and threading: a sink is not owned by the caller side. A transport
// must hold a valid sink for the whole attachment, must detach() before the
// sink is destroyed, must confine every sink call to the single thread that
// performed attach (the registry is deliberately not synchronized), and must
// not re-attach while attached. Implementations of this interface are the
// registry and nothing else; the header is installed only so the QtDBus
// adapter milestone can depend on the narrow type instead of the concrete
// registry.
class StatusNotifierEventSink
{
public:
    virtual ~StatusNotifierEventSink() = default;

    // Allocates a fresh, globally unique generation for `uniqueName` and
    // marks it live. Re-basing a still-live name drops that owner's items
    // deterministically (a watcher rebaseline). Returns 0 on any failure,
    // including a non-owner name, owner-table exhaustion, or counter
    // exhaustion; callers must treat 0 as "event refused".
    [[nodiscard]] virtual quint64 beginOwnerGeneration(const QString &uniqueName) = 0;

    // Retires a live owner only when `expectedGeneration` matches its current
    // generation; a stale loss event is refused and the owner plus its items
    // survive untouched.
    [[nodiscard]] virtual RegistryOutcome ownerLost(const QString &uniqueName,
                                                    quint64 expectedGeneration) = 0;

    [[nodiscard]] virtual RegistryOutcome registerItem(const OwnerKey &key,
                                                       const ItemDescriptor &descriptor) = 0;
    [[nodiscard]] virtual RegistryOutcome removeItem(const OwnerKey &key) = 0;
    [[nodiscard]] virtual RegistryOutcome removeAllForOwner(const QString &uniqueName,
                                                            quint64 generation) = 0;

    // Marks the watcher's item population as observed once; presentation may
    // leave Loading only afterwards.
    virtual void markInitialPopulationComplete() = 0;

    // Starts a new watcher epoch: the population bit resets so presentation
    // returns to fail-closed Loading until the replacement watcher's items
    // are observed.
    virtual void beginWatcherEpoch() = 0;

protected:
    StatusNotifierEventSink() = default;
    StatusNotifierEventSink(const StatusNotifierEventSink &) = default;
    StatusNotifierEventSink &operator=(const StatusNotifierEventSink &) = delete;
};

} // namespace QindaQt::StatusNotifier
