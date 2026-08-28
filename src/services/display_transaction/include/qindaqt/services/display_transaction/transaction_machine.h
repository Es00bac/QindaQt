// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_transaction/transaction_journal.h>
#include <qindaqt/services/display_transaction/transaction_ports.h>

namespace QindaQt::DisplayTransaction
{

class Machine final
{
public:
    // The clock and port are borrowed, are never invoked off the constructing
    // thread, and must outlive this object. Machine owns only bounded values.
    // Commands are synchronous value transitions. A rejected result preserves
    // view(), currentSnapshot(), and activeJournal() exactly. For accepted
    // results, stateChanged is true exactly when the view or accepted snapshot
    // changed; port call counts are not machine state. lastTerminalReason is
    // retained in Ready until the next successful stage. Async effects cross
    // only through token-fenced SideEffectPort requests.
    Machine(MonotonicClock &clock, SideEffectPort &port, Timing timing = {});

    [[nodiscard]] const MachineView &view() const noexcept;
    [[nodiscard]] const Display::Snapshot &currentSnapshot() const noexcept;
    [[nodiscard]] const Journal &activeJournal() const noexcept;

    CommandResult initialize(const Display::Snapshot &snapshot, SafetyState safety);
    CommandResult stage(const QString &transactionId, const Display::Candidate &candidate);
    CommandResult preview(const QString &transactionId);
    CommandResult confirm(const QString &transactionId);
    CommandResult cancel(const QString &transactionId);
    CommandResult applyCompleted(quint64 token, ApplyOutcome outcome);
    CommandResult observedSnapshot(const Display::Snapshot &snapshot);
    CommandResult externalIntentObserved(const Display::Snapshot &snapshot);
    CommandResult topologyChanged(const Display::Snapshot &snapshot);
    CommandResult topologySettled(const Display::Snapshot &snapshot);
    CommandResult safetyChanged(SafetyState safety);
    CommandResult prepareForSuspend();
    CommandResult tick();
    CommandResult recover(const Journal &journal, const Display::Snapshot &snapshot,
                          SafetyState safety);
    CommandResult retryStuck();

private:
    friend class MachineTestAccess;

    [[nodiscard]] CommandResult rejected(CommandError error) const;
    [[nodiscard]] CommandResult accepted(bool changed, CommandError error = CommandError::None) const;
    [[nodiscard]] bool validSnapshot(const Display::Snapshot &snapshot) const;
    [[nodiscard]] bool transactionMatches(const QString &transactionId) const;
    [[nodiscard]] quint64 nextToken();
    void setState(MachineState state);
    void clearTransaction();
    void beginForwardApply();
    void requestRevert(Display::TransactionReason reason);
    void beginRevert(Display::TransactionReason reason);
    void issueRevertApply();
    void scheduleRevertRetry();
    void enterStuck(bool cleanupOnly = false);
    void finishReady(const Display::Snapshot &snapshot);
    [[nodiscard]] bool snapshotMatches(const Display::Snapshot &snapshot,
                                       const Display::Candidate &candidate) const;
    [[nodiscard]] bool snapshotMatchesSurvivingProperties(
        const Display::Snapshot &snapshot) const;
    [[nodiscard]] ApplyRequest makeRevertRequest(quint64 token) const;
    [[nodiscard]] QList<SurvivingOutputProperties> survivingProperties(
        const Display::Snapshot &snapshot) const;
    [[nodiscard]] bool sameOutputSet(const Display::Snapshot &snapshot,
                                     const Display::Candidate &candidate) const;
    [[nodiscard]] bool sameOutputSet(const Display::Snapshot &left,
                                     const Display::Snapshot &right) const;

    MonotonicClock &m_clock;
    SideEffectPort &m_port;
    Timing m_timing;
    MachineView m_view;
    Display::Snapshot m_snapshot;
    Display::Candidate m_staged;
    Display::Candidate m_preimage;
    Journal m_journal;
    QList<SurvivingOutputProperties> m_survivingProperties;
    quint64 m_nextToken = 1;
    quint64 m_activeToken = 0;
    bool m_revertRequested = false;
    bool m_abandonAfterSettle = false;
    bool m_cleanupOnlyStuck = false;
};

} // namespace QindaQt::DisplayTransaction
