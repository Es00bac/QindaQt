// SPDX-License-Identifier: GPL-3.0-or-later

// Shared scripted client seams for the Clipboard applet controller tests.
// Split by behavior: tst_clipboard_applet_controller.cpp covers lifecycle,
// authority, and capability gating; tst_clipboard_applet_fencing.cpp covers
// reply/completion attribution fencing against hostile seams.

#pragma once

#include <qindaqt/services/clipboard_model/clipboard_history.h>
#include <qindaqt/shell/clipboard_applet/clipboard_model_client_adapter.h>

#include <QtCore/QList>

namespace QindaQt::ShellClipboardApplet::Tests {

using QindaQt::Services::ClipboardModel::ClearScope;
using QindaQt::Services::ClipboardModel::EntryId;
using QindaQt::Services::ClipboardModel::HistorySnapshot;
using QindaQt::Services::ClipboardModel::SearchOutcome;

// A descriptor that passes the C0 hostile-input admission floor: valid
// generation-tagged identity, canonical storable media, exact 32-byte
// fingerprint, sanitized bounded label/preview. AGENT-GUARD: every fixture a
// test serves to the controller starts floor-valid so a rejection always
// proves the specific fence under test, never the floor itself; hostile rows
// corrupt one field at a time from this baseline.
inline QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor floorValidDescriptor(
    quint32 generation, quint32 serial, const QString &preview)
{
    QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor descriptor;
    descriptor.id = { generation, serial };
    descriptor.preview = preview;
    descriptor.sourceLabel = QStringLiteral("source");
    descriptor.formats = { { QStringLiteral("text/plain"),
                             qint64(qMax(1, preview.size())) } };
    descriptor.fingerprint = QByteArray(32, 'a');
    return descriptor;
}

// Scripted client seam issuing deliberately unique-but-unordered request ids
// and delivering replies in caller-chosen order. AGENT-GUARD: the public seam
// promises id uniqueness only — these ids reproduce the exact disorder a real
// async transport can produce, which the controller must fence with its own
// monotonic query generation rather than id arithmetic.
class UnorderedFakeClient final : public ClipboardClientInterface {
public:
    quint64 m_nextSearchRequestId = 0;
    QList<quint64> m_issuedSearchRequestIds;

    HistorySnapshot m_snapshot;
    bool m_locked = false;

    [[nodiscard]] ClientState clientState() const noexcept override { return ClientState::Ready; }
    [[nodiscard]] QString reasonCode() const override { return {}; }
    [[nodiscard]] QString owner() const override { return QStringLiteral("fake"); }
    [[nodiscard]] bool isOwnerAvailable() const noexcept override { return true; }
    [[nodiscard]] bool isLocked() const noexcept override { return m_locked; }
    [[nodiscard]] HistorySnapshot snapshot() const override { return m_snapshot; }

    quint64 requestPromote(EntryId, quint32, quint64) override { return 0; }
    quint64 requestRemove(EntryId, quint32) override { return 0; }
    quint64 requestSetPinned(EntryId, bool, quint32) override { return 0; }
    quint64 requestClear(ClearScope, quint32) override { return 0; }

    quint64 requestSearch(const QString &, quint32, int) override
    {
        // Unique, strictly unordered: even requests descend from 902,
        // odd requests ascend from 101, so consecutive ids are never
        // monotonically comparable in either direction.
        const quint64 id = (m_nextSearchRequestId % 2 == 0)
            ? quint64(902 - 100 * (m_nextSearchRequestId / 2))
            : quint64(101 + 100 * (m_nextSearchRequestId / 2));
        ++m_nextSearchRequestId;
        m_issuedSearchRequestIds.append(id);
        return id;
    }

    void deliverSearchReply(quint64 requestId, const SearchOutcome &outcome)
    {
        Q_EMIT searchCompleted(requestId, outcome);
    }

    void deliverLock(bool locked)
    {
        m_locked = locked;
        Q_EMIT lockStateChanged(locked);
    }
};

// Hostile/async scripted seam for synchronous-flush and completion-injection
// attacks. Every request id is unique; replies and completions are delivered
// exactly when the test chooses, including re-entrantly inside dispatch calls
// (the P1 vector: flushing a queued superseded reply inside requestSearch()).
class HostileScriptedClient final : public ClipboardClientInterface {
public:
    struct RecordedOperation {
        OperationKind kind = OperationKind::Promote;
        quint64 requestId = 0;
        quint64 tick = 0;
        EntryId id;
    };

    quint64 m_nextRequestId = 500; // deliberately unrelated to search ids
    QList<RecordedOperation> m_operations;
    quint64 m_queuedSearchRequestId = 0;
    SearchOutcome m_queuedSearchOutcome;
    bool m_flushQueuedReplyDuringNextSearch = false;
    bool m_answerSearchSynchronously = false;
    SearchOutcome m_scriptedSearchOutcome;
    bool m_completeOperationsSynchronously = false;
    OperationOutcome m_scriptedCompletion;
    // When armed, record() first re-entrantly emits this completion — for an
    // EARLIER request id — inside the dispatch call, then proceeds. Exercises
    // exact-id attribution of cross-request synchronous flushes.
    bool m_flushEarlierCompletionDuringRecord = false;
    quint64 m_flushedEarlierRequestId = 0;
    OperationOutcome m_flushedEarlierCompletion;

    HistorySnapshot m_snapshot;
    bool m_locked = false;

    [[nodiscard]] ClientState clientState() const noexcept override { return ClientState::Ready; }
    [[nodiscard]] QString reasonCode() const override { return {}; }
    [[nodiscard]] QString owner() const override { return QStringLiteral("hostile-fake"); }
    [[nodiscard]] bool isOwnerAvailable() const noexcept override { return true; }
    [[nodiscard]] bool isLocked() const noexcept override { return m_locked; }
    [[nodiscard]] HistorySnapshot snapshot() const override { return m_snapshot; }

    quint64 requestPromote(EntryId id, quint32, quint64 tick) override
    {
        return record(OperationKind::Promote, id, tick);
    }
    quint64 requestRemove(EntryId id, quint32) override
    {
        return record(OperationKind::Remove, id, 0);
    }
    quint64 requestSetPinned(EntryId id, bool, quint32) override
    {
        return record(OperationKind::SetPinned, id, 0);
    }
    quint64 requestClear(ClearScope, quint32) override
    {
        return record(OperationKind::Clear, {}, 0);
    }

    quint64 requestSearch(const QString &, quint32, int) override
    {
        const quint64 id = ++m_nextRequestId;
        if (m_flushQueuedReplyDuringNextSearch) {
            // Tarski's P1-2 vector: the stale queued reply is flushed
            // re-entrantly while the new request is being issued, before
            // the controller can know the new request id.
            m_flushQueuedReplyDuringNextSearch = false;
            Q_EMIT searchCompleted(m_queuedSearchRequestId, m_queuedSearchOutcome);
        }
        if (m_answerSearchSynchronously) {
            // Benign synchronous seam: answers with the id of the request
            // currently being issued, inside the call itself.
            Q_EMIT searchCompleted(id, m_scriptedSearchOutcome);
        }
        return id;
    }

    void emitSnapshot()
    {
        Q_EMIT snapshotChanged(m_snapshot);
    }

    void queueSearchReply(quint64 requestId, const SearchOutcome &outcome)
    {
        m_queuedSearchRequestId = requestId;
        m_queuedSearchOutcome = outcome;
    }

    void emitSearchReply(quint64 requestId, const SearchOutcome &outcome)
    {
        Q_EMIT searchCompleted(requestId, outcome);
    }

    void emitCompletion(quint64 requestId, const OperationOutcome &outcome)
    {
        Q_EMIT operationCompleted(requestId, outcome);
    }

private:
    quint64 record(OperationKind kind, EntryId id, quint64 tick)
    {
        const quint64 requestId = ++m_nextRequestId;
        RecordedOperation recorded;
        recorded.kind = kind;
        recorded.requestId = requestId;
        recorded.tick = tick;
        recorded.id = id;
        m_operations.append(recorded);

        if (m_flushEarlierCompletionDuringRecord) {
            m_flushEarlierCompletionDuringRecord = false;
            Q_EMIT operationCompleted(m_flushedEarlierRequestId, m_flushedEarlierCompletion);
        }
        if (m_completeOperationsSynchronously) {
            OperationOutcome completion = m_scriptedCompletion;
            completion.id = id;
            Q_EMIT operationCompleted(requestId, completion);
        }
        return requestId;
    }
};

} // namespace QindaQt::ShellClipboardApplet::Tests
