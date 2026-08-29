// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_transaction/transaction_types.h>

namespace QindaQt::DisplayTransaction
{

enum class JournalMutationOutcome {
    // No durable pathname state changed. The caller may safely reason from its
    // prior journal truth.
    Unchanged,
    // The requested journal value or absence crossed every supported
    // durability barrier.
    Durable,
    // The pathname commit occurred, but its directory durability barrier
    // failed. The state may be old or new after a crash; this must never
    // authorize a forward compositor mutation.
    DurabilityUncertain,
};

class MonotonicClock
{
public:
    virtual ~MonotonicClock() = default;
    // Values are nondecreasing within one Machine lifetime; they need no wall
    // clock epoch and may repeat. Machine calls this only on its owning thread.
    [[nodiscard]] virtual quint64 nowMilliseconds() const noexcept = 0;
};

class SideEffectPort
{
public:
    virtual ~SideEffectPort() = default;

    // AGENT-CONTRACT: Implementations are borrowed by Machine, remain
    // addressable (even while their external transport is unavailable), and
    // outlive it. Calls occur on the constructing thread, return without
    // synchronously re-entering Machine, and never retain references to their
    // arguments. storeJournal/clearJournal are synchronous and atomic until
    // their pathname commit point. `Unchanged` guarantees prior durable truth,
    // `Durable` proves the requested truth, and `DurabilityUncertain` means the
    // pathname changed before a failed durability barrier. Only `Durable`
    // permits a forward apply; uncertainty remains conservative cleanup or
    // restart-recovery truth. requestApply accepts one immutable request and
    // may later produce zero or one applyCompleted callback with the exact
    // token. A late callback is permitted and will be rejected; timeout never
    // authorizes replay of a forward request. Disconnect is represented by
    // TransportUncertain or no callback, never by swapping the port object.
    //
    // AGENT-CONTRACT: The machine owner must redeliver the current live
    // snapshot through observedSnapshot after every apply callback and every
    // apply deadline, even when the compositor revision did not change. A
    // changed output set is routed through topologyChanged in every active
    // state. Callback-first ordering is the D1 port assumption; D2 must prove
    // compositor callback/device ordering and the cross-client in-flight
    // intent window rather than treating fake-port evidence as runtime proof.
    // While Staged, a same-set external change is routed through
    // externalIntentObserved (ordinary observedSnapshot is invalid there) so
    // a stale candidate cannot preview. While SettlingTopology, never route
    // platform post-hotplug observations through externalIntentObserved; use
    // observedSnapshot/topologyChanged until explicit topologySettled.
    [[nodiscard]] virtual JournalMutationOutcome storeJournal(
        const Journal &journal) = 0;
    [[nodiscard]] virtual JournalMutationOutcome clearJournal() = 0;
    virtual void requestApply(const ApplyRequest &request) = 0;
};

} // namespace QindaQt::DisplayTransaction
