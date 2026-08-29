// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPair>
#include <QtCore/QSet>
#include <QtCore/QVariantList>
#include <qindaqt/services/clipboard_model/clipboard_types.h>
#include "qindaqt/shell/clipboard_applet/clipboard_applet_types.h"
#include "qindaqt/shell/clipboard_applet/clipboard_client_interface.h"

namespace QindaQt::ShellClipboardApplet {

// AGENT-CONTRACT: Composed shell facade exposed to QML.
// It translates user gestures into bounded client intents, enforces generation
// and privacy fencing, tracks pending in-flight requests, and reprojects the
// presentation model. It never performs live Wayland operations or direct memory
// access on Clipboard service internals.
class ClipboardAppletController : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString phaseText READ phaseText NOTIFY stateReprojected)
    Q_PROPERTY(QString phaseReasonText READ phaseReasonText NOTIFY stateReprojected)
    Q_PROPERTY(bool isLocked READ isLocked NOTIFY stateReprojected)
    Q_PROPERTY(bool isHistoryEnabled READ isHistoryEnabled NOTIFY stateReprojected)
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
    explicit ClipboardAppletController(ClipboardClientInterface *client, QObject *parent = nullptr);
    ~ClipboardAppletController() override = default;

    [[nodiscard]] QString phaseText() const noexcept;
    [[nodiscard]] QString phaseReasonText() const noexcept;
    [[nodiscard]] bool isLocked() const noexcept;
    [[nodiscard]] bool isHistoryEnabled() const noexcept;
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

    void reproject();
    void setFeedback(const QString &message, const QString &status = QStringLiteral("error"));
    void cancelPendingForGeneration(quint32 oldGeneration);
    void dispatchSearch();
    void abandonSearch();
    void applySearchOutcome(const QindaQt::Services::ClipboardModel::SearchOutcome &outcome);
    void applyOperationOutcome(const OperationOutcome &outcome);
    void noteObservedTicks(const QindaQt::Services::ClipboardModel::HistorySnapshot &snapshot);

    ClipboardClientInterface *m_client = nullptr;
    QindaQt::Services::ClipboardModel::HistorySnapshot m_snapshot;
    ClipboardAppletProjection m_projection;

    bool m_isSearchActive = false;
    QString m_searchQuery;
    QList<QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor> m_searchResults;
    bool m_searchTruncated = false;
    // AGENT-GUARD: reply freshness is fenced by this controller-internal
    // monotonically increasing query generation, never by ordering of
    // client-supplied request ids — the client seam promises uniqueness
    // only. Every in-flight request id maps to the generation that issued
    // it; replies carrying any other generation are dropped.
    quint64 m_searchQueryGeneration = 0;
    QHash<quint64, quint64> m_pendingSearchRequests;

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
    // snapshot so issued ticks are strictly increasing across the lineage.
    quint64 m_nextPromoteTick = 1;

    bool m_feedbackPresent = false;
    QString m_feedback;
    QString m_feedbackStatus = QStringLiteral("error");
};

} // namespace QindaQt::ShellClipboardApplet
