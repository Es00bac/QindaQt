// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QSet>
#include <QtCore/QVariantList>
#include <functional>
#include <qindaqt/services/clipboard_model/clipboard_types.h>
#include "qindaqt/shell/clipboard_applet/clipboard_applet_types.h"
#include "qindaqt/shell/clipboard_applet/clipboard_client_interface.h"

namespace QindaQt::ShellClipboardApplet {

// AGENT-CONTRACT: Composed shell facade exposed to QML.
// It translates user gestures into bounded client intents, enforces generation
// and privacy fencing, tracks pending in-flight requests, and reprojects the
// presentation model. It never performs live Wayland operations or direct memory
// access on Clipboard service internals.
//
// Least authority: the composing shell passes the audited manifest/policy
// capability grants at construction. `clipboardReadGranted == false` withholds
// all observation (phase reports unavailable, no entries are retained);
// `clipboardWriteGranted == false` keeps browsing but refuses every mutating
// intent before dispatch. Both gates fail closed and cannot change after
// construction, mirroring the Power applet's read/control split.
class ClipboardAppletController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateReprojected)
    Q_PROPERTY(QString phaseReasonText READ phaseReasonText NOTIFY stateReprojected)
    Q_PROPERTY(bool isLocked READ isLocked NOTIFY stateReprojected)
    Q_PROPERTY(bool isHistoryEnabled READ isHistoryEnabled NOTIFY stateReprojected)
    Q_PROPERTY(bool clipboardReadGranted READ clipboardReadGranted CONSTANT)
    Q_PROPERTY(bool clipboardWriteGranted READ clipboardWriteGranted CONSTANT)
    Q_PROPERTY(QVariantList entryRows READ entryRows NOTIFY stateReprojected)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY stateReprojected)
    Q_PROPERTY(int pinnedCount READ pinnedCount NOTIFY stateReprojected)
    Q_PROPERTY(int unpinnedCount READ unpinnedCount NOTIFY stateReprojected)
    Q_PROPERTY(qint64 totalPayloadBytes READ totalPayloadBytes NOTIFY stateReprojected)
    Q_PROPERTY(QString totalPayloadBytesFormatted READ totalPayloadBytesFormatted NOTIFY stateReprojected)
    Q_PROPERTY(bool isSearchActive READ isSearchActive NOTIFY stateReprojected)
    Q_PROPERTY(QString searchQuery READ searchQuery NOTIFY stateReprojected)
    Q_PROPERTY(int searchResultCount READ searchResultCount NOTIFY stateReprojected)
    Q_PROPERTY(bool searchTruncated READ searchTruncated NOTIFY stateReprojected)
    Q_PROPERTY(QString emptyReasonText READ emptyReasonText NOTIFY stateReprojected)
    Q_PROPERTY(int pendingOperationCount READ pendingOperationCount NOTIFY stateReprojected)
    Q_PROPERTY(bool feedbackPresent READ feedbackPresent NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedback READ feedback NOTIFY feedbackChanged)
    Q_PROPERTY(QString feedbackStatus READ feedbackStatus NOTIFY feedbackChanged)

public:
    explicit ClipboardAppletController(ClipboardClientInterface *client,
                                       bool clipboardReadGranted,
                                       bool clipboardWriteGranted,
                                       QObject *parent = nullptr);
    ~ClipboardAppletController() override = default;

    [[nodiscard]] QString phaseText() const noexcept;
    [[nodiscard]] QString phaseReasonText() const noexcept;
    [[nodiscard]] bool isLocked() const noexcept;
    [[nodiscard]] bool isHistoryEnabled() const noexcept;
    [[nodiscard]] bool clipboardReadGranted() const noexcept { return m_clipboardReadGranted; }
    [[nodiscard]] bool clipboardWriteGranted() const noexcept { return m_clipboardWriteGranted; }
    [[nodiscard]] QVariantList entryRows() const;
    [[nodiscard]] int entryCount() const noexcept;
    [[nodiscard]] int pinnedCount() const noexcept;
    [[nodiscard]] int unpinnedCount() const noexcept;
    [[nodiscard]] qint64 totalPayloadBytes() const noexcept;
    [[nodiscard]] QString totalPayloadBytesFormatted() const;
    [[nodiscard]] bool isSearchActive() const noexcept;
    [[nodiscard]] QString searchQuery() const;
    [[nodiscard]] int searchResultCount() const noexcept;
    [[nodiscard]] bool searchTruncated() const noexcept;
    [[nodiscard]] QString emptyReasonText() const;
    [[nodiscard]] int pendingOperationCount() const noexcept;
    [[nodiscard]] bool feedbackPresent() const noexcept;
    [[nodiscard]] QString feedback() const;
    [[nodiscard]] QString feedbackStatus() const;

    [[nodiscard]] const ClipboardAppletProjection &projection() const noexcept { return m_projection; }

    Q_INVOKABLE bool selectEntry(quint32 generation, quint32 serial);
    Q_INVOKABLE bool deleteEntry(quint32 generation, quint32 serial);
    Q_INVOKABLE bool togglePin(quint32 generation, quint32 serial);
    Q_INVOKABLE bool clearHistory(bool unpinnedOnly);
    Q_INVOKABLE void setSearchQuery(const QString &query);
    Q_INVOKABLE void clearSearch();
    Q_INVOKABLE void clearFeedback();

Q_SIGNALS:
    void stateReprojected();
    void feedbackChanged();

private Q_SLOTS:
    void onStateChanged(QindaQt::ShellClipboardApplet::ClientState state, const QString &reasonCode);
    void onSnapshotChanged(const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot);
    void onLockStateChanged(bool locked);
    void onOperationCompleted(quint64 requestId, const QindaQt::ShellClipboardApplet::OperationOutcome &outcome);
    void onSearchCompleted(quint64 requestId, const QindaQt::Services::ClipboardModel::SearchOutcome &outcome);

private:
    struct PendingRequest {
        OperationKind kind = OperationKind::Promote;
        QindaQt::Services::ClipboardModel::EntryId id;
        quint32 generation = 0;
    };

    struct PendingSearchRequest {
        quint64 queryGeneration = 0;
        quint32 snapshotGeneration = 0;
        quint64 snapshotRevision = 0;
    };

    void reproject();
    void setFeedback(const QString &message, const QString &status = QStringLiteral("error"));
    void cancelPendingForGeneration(quint32 oldGeneration);
    void acceptSnapshot(const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot);
    void dropAcceptedBaseline();
    void rejectSnapshot();
    void dispatchSearch();
    void abandonSearch();
    void applySearchOutcome(
        const QindaQt::Services::ClipboardModel::SearchOutcome &outcome,
        const PendingSearchRequest &request);
    void rememberRejectedLineage(
        const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot,
        bool generationMustAdvance = false);
    void resolveCompletion(quint64 requestId, const OperationOutcome &outcome);
    void noteObservedTicks(const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot);
    void drainDeferredSignals();
    quint64 dispatchOperation(
        OperationKind kind,
        QindaQt::Services::ClipboardModel::EntryId id,
        quint32 generation,
        const std::function<quint64(ClipboardClientInterface *)> &invoke);
    [[nodiscard]] bool refuseMutation();

    ClipboardClientInterface *m_client = nullptr;
    bool m_clipboardReadGranted = false;
    bool m_clipboardWriteGranted = false;
    QindaQt::Services::ClipboardModel::HistorySnapshot m_snapshot;
    ClipboardAppletProjection m_projection;

    // AGENT-GUARD: snapshot admission baseline. Every accepted snapshot must
    // be delivered under the recorded owner and must not regress the recorded
    // generation high-water OR the lifetime revision high-water. C0 never
    // resets revision when a purge advances generation, so comparing the pair
    // lexicographically would admit forged content after a revision regression.
    // On owner loss or
    // replacement the baseline is dropped and the next snapshot under the new
    // owner must be content-empty: volatile history starts empty per owner,
    // so non-empty content under a fresh owner is foreign content.
    bool m_hasBaseline = false;
    bool m_baselineDroppedForOwner = false;
    QString m_baselineOwner;
    quint32 m_baselineGeneration = 0;
    quint64 m_baselineRevision = 0;
    // A same-generation authority purge is valid only at UINT32_MAX. It proves
    // C0 pinned the generation and latched content-operation exhaustion. Keep
    // that state distinct from hostile-snapshot rejection: the current owner
    // is unavailable-until-restart, while a fresh owner baseline recovers.
    bool m_lineageExhausted = false;
    // A structurally impossible snapshot poisons its exact lineage. Merely
    // flipping privacy/capability flags at the same generation/revision may
    // not turn rejected content into presentable content; recovery requires a
    // later valid snapshot (or a new owner baseline).
    bool m_hasRejectedLineage = false;
    quint32 m_rejectedGeneration = 0;
    quint64 m_rejectedRevision = 0;
    bool m_rejectedGenerationMustAdvance = false;
    // True while the last incoming snapshot was refused by an admission fence;
    // presentation withholds everything until a fresh valid snapshot lands.
    bool m_snapshotRejected = false;

    bool m_isSearchActive = false;
    QString m_searchQuery;
    QList<QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor> m_searchResults;
    bool m_searchTruncated = false;
    // AGENT-GUARD: reply freshness is fenced by this controller-internal
    // monotonically increasing query generation, never by ordering of
    // client-supplied request ids — the client seam promises uniqueness
    // only. Every in-flight request id maps to the query generation and exact
    // snapshot lineage that issued it; replies carrying any other attribution
    // are dropped.
    quint64 m_searchQueryGeneration = 0;
    QHash<quint64, PendingSearchRequest> m_pendingSearchRequests;

    // AGENT-GUARD: seams may emit operationCompleted/searchCompleted
    // synchronously INSIDE the dispatch call, before the returned request id
    // is knowable. Such signals are buffered while m_insideClientCall is set
    // and attributed afterwards strictly by request id — a hostile or queued
    // reply for a superseded request flushed during the call can never match
    // the id of the request being issued and is dropped.
    bool m_insideClientCall = false;
    QList<QPair<quint64, QindaQt::Services::ClipboardModel::SearchOutcome>> m_deferredSearchReplies;
    QList<QPair<quint64, OperationOutcome>> m_deferredCompletions;

    QHash<quint64, PendingRequest> m_pendingRequests;
    QSet<QPair<quint32, quint32>> m_pendingEntries;

    // AGENT-GUARD: promote ticks are controller-issued monotonic metadata
    // the model trusts for recency ordering; wall-clock time can step
    // backwards. The counter is raised above every tick observed in a
    // snapshot so issued ticks are strictly increasing across the lineage,
    // and fails closed at the fixed-width ceiling: selectEntry refuses a
    // promote rather than issuing a wrapped (non-monotonic) tick.
    quint64 m_nextPromoteTick = 1;

    bool m_feedbackPresent = false;
    QString m_feedback;
    QString m_feedbackStatus = QStringLiteral("error");
};

} // namespace QindaQt::ShellClipboardApplet
