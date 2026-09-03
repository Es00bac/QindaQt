// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_color_assignment/assignment_document.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QObject>
#include <QVariantMap>

#include <optional>

namespace QindaQt::DisplayColor
{

enum class DocumentAvailability : quint32
{
    // The client has no confirmed Settings1 authority yet (never connected,
    // transport lost, or owner replacement in progress). A draft is rejected.
    Unavailable = 0,
    // A confirmed snapshot decoded into a usable document.
    Ready = 1,
    // A confirmed snapshot exists, but its assignment value is hostile or
    // from an unknown document shape. Draft application is refused so
    // unknown-format data can never be silently overwritten.
    UnusableDocument = 2,
};

struct AssignmentDocumentView
{
    DocumentAvailability availability = DocumentAvailability::Unavailable;
    QString reasonCode;
    // Last decoded document; when availability is Unavailable this is the
    // last confirmed decode (possibly empty), never invented truth.
    AssignmentDocument document;
    QString epoch;
    quint64 revision = 0;

    friend bool operator==(const AssignmentDocumentView &,
                           const AssignmentDocumentView &) = default;
};

enum class ApplyStatus : quint32
{
    // The service applied the write and the authoritative persisted value
    // decoded back to the expected document truth.
    Applied = 0,
    // Applied with unchanged revision and no changed keys: the merged
    // document already equaled the persisted one.
    AppliedNoOp = 1,
    // The service rejected the optimistic base revision; the caller must
    // rebase on a refreshed document.
    Conflict = 2,
    // The write was sent but its outcome could not be proven (timeout, bus
    // loss, or any reply the client could not validate). Never replayed
    // automatically; the caller must resync and re-apply.
    //
    // AGENT-NOTE: A Settings1 service-epoch change can never arrive as a
    // commit outcome — the exact-owner client rejects epoch-mismatched
    // replies as incoherent lineage and reports the write uncertain. An
    // epoch change is therefore observed through document() becoming
    // Unavailable, plus Uncertain for any in-flight apply.
    Uncertain = 3,
    // The service returned any other typed rejection.
    Failed = 4,
};

struct AssignmentApplyOutcome
{
    ApplyStatus status = ApplyStatus::Failed;
    QString reasonCode;
    quint64 revisionAfter = 0;
    // Authoritative persisted document when status is Applied/AppliedNoOp.
    AssignmentDocument persistedDocument;

    friend bool operator==(const AssignmentApplyOutcome &,
                           const AssignmentApplyOutcome &) = default;
};

// AGENT-CONTRACT: One store owns one exact Settings1 lineage for the
// "displays.colorAssignments" key through the caller-supplied client. The
// composition root must give this store its own client (scoped to that key)
// and serialize calls on the owner thread; the store never starts, stops,
// or refreshes the client. Draft application is optimistic and fenced: a
// conflict or epoch mismatch is reported, never auto-retried, and an
// uncertain write is never replayed — persistence truth stays with the
// Settings1 service, and the C0 revisioned model remains the only applied-
// assignment authority.
class SettingsAssignmentStore final : public QObject
{
    Q_OBJECT
public:
    explicit SettingsAssignmentStore(
        QindaQt::Services::SettingsClient::SettingsClient &client,
        QObject *parent = nullptr);
    ~SettingsAssignmentStore() override;

    [[nodiscard]] AssignmentDocumentView document() const;

    // Validates the draft against the current confirmed document and, on
    // success, sends the merged document as one optimistic Settings1
    // transaction. Returns false with *error set when the draft is invalid
    // (invalid-draft/…), there is no confirmed usable authority
    // (unavailable / document-unusable/…), a write is already in flight
    // (write-in-flight), or the client refused the write (transport-
    // rejected); nothing is sent in those cases. The typed outcome of an
    // accepted write arrives on applyFinished.
    bool applyDraft(const ColorAssignmentDraft &draft, QString *error = nullptr);

    [[nodiscard]] bool writeInFlight() const;

Q_SIGNALS:
    void documentChanged();
    void applyFinished(const QindaQt::DisplayColor::AssignmentApplyOutcome &outcome);

private:
    void refreshDocumentView();
    void handleCommitFinished(const QindaQt::Services::SettingsClient::CommitOutcome &outcome);

    QindaQt::Services::SettingsClient::SettingsClient &m_client;
    AssignmentDocumentView m_view;
    std::optional<AssignmentDocument> m_expectedDocument;
    bool m_applyInFlight = false;
};

} // namespace QindaQt::DisplayColor

Q_DECLARE_METATYPE(QindaQt::DisplayColor::AssignmentApplyOutcome)
