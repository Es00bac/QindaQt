// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/display_color_assignment/assignment_store.h>

#include <QtCore/QMetaType>

namespace QindaQt::DisplayColor
{

namespace Scn = QindaQt::Services::SettingsClient;
using Scn::ClientState;
using Scn::CommitOutcome;
using Scn::SettingsSnapshot;
using Services::SettingsProtocol::SettingsWireStatus;

namespace
{

QString wireStatusName(SettingsWireStatus status)
{
    return Services::SettingsProtocol::settingsWireStatusName(status);
}

} // namespace

SettingsAssignmentStore::SettingsAssignmentStore(
    QindaQt::Services::SettingsClient::SettingsClient &client, QObject *parent)
    : QObject(parent), m_client(client)
{
    qRegisterMetaType<AssignmentApplyOutcome>();

    connect(&m_client, &Scn::SettingsClient::stateChanged, this,
            &SettingsAssignmentStore::refreshDocumentView);
    connect(&m_client, &Scn::SettingsClient::snapshotChanged, this,
            &SettingsAssignmentStore::refreshDocumentView);
    connect(&m_client, &Scn::SettingsClient::commitFinished, this,
            &SettingsAssignmentStore::handleCommitFinished);
    connect(&m_client, &Scn::SettingsClient::commitUncertain, this, [this](const QString &) {
        if (!m_applyInFlight) {
            return;
        }
        // AGENT-GUARD: An uncertain write is terminal for this apply attempt.
        // The transport may or may not have committed; replaying here could
        // double-apply a document the service already accepted, so the
        // outcome is reported and the caller must resync and re-apply
        // explicitly (Settings1 no-replay truth).
        m_applyInFlight = false;
        m_expectedDocument.reset();
        AssignmentApplyOutcome outcome;
        outcome.status = ApplyStatus::Uncertain;
        outcome.reasonCode = QStringLiteral("uncertain-write");
        Q_EMIT applyFinished(outcome);
    });

    // Adopt authority that already exists at construction: a client that is
    // Ready before this store is composed must be reflected without waiting
    // for the next state/snapshot signal.
    refreshDocumentView();
}

SettingsAssignmentStore::~SettingsAssignmentStore() = default;

AssignmentDocumentView SettingsAssignmentStore::document() const
{
    return m_view;
}

bool SettingsAssignmentStore::writeInFlight() const
{
    return m_applyInFlight;
}

void SettingsAssignmentStore::refreshDocumentView()
{
    AssignmentDocumentView next = m_view;
    next.availability = DocumentAvailability::Unavailable;
    next.reasonCode = QStringLiteral("client-not-ready");

    const auto snapshot = m_client.snapshot();
    if (m_client.state() == ClientState::Ready && snapshot.has_value()) {
        next.epoch = snapshot->epoch;
        next.revision = snapshot->revision;
        const QVariant value = snapshot->values.value(QLatin1String(ColorAssignmentsSettingsKey));
        if (!value.isValid()) {
            next.availability = DocumentAvailability::UnusableDocument;
            next.reasonCode = QStringLiteral("assignment-key-absent");
        } else {
            const AssignmentDocumentDecodeResult decoded = decodeAssignmentDocument(value);
            if (decoded.ok) {
                next.availability = DocumentAvailability::Ready;
                next.reasonCode.clear();
                next.document = decoded.document;
            } else {
                next.availability = DocumentAvailability::UnusableDocument;
                next.reasonCode = QStringLiteral("document-unusable/") + decoded.reasonCode;
            }
        }
    }

    if (next != m_view) {
        m_view = next;
        Q_EMIT documentChanged();
    }
}

bool SettingsAssignmentStore::applyDraft(const ColorAssignmentDraft &draft, QString *error)
{
    const auto reject = [this, &error](const QString &reason) {
        if (error != nullptr) {
            *error = reason;
        }
        return false;
    };

    if (m_applyInFlight) {
        return reject(QStringLiteral("write-in-flight"));
    }

    const ColorAssignmentDraftValidation validation = validateColorAssignmentDraft(draft);
    if (!validation.ok) {
        return reject(QStringLiteral("invalid-draft/") + validation.reasonCode);
    }

    const AssignmentDocumentView view = document();
    if (view.availability == DocumentAvailability::Unavailable) {
        return reject(QStringLiteral("unavailable"));
    }
    if (view.availability == DocumentAvailability::UnusableDocument) {
        // AGENT-GUARD: Refusing to merge into an unknown document shape is
        // what keeps a future/foreign document format from being silently
        // overwritten by this lane. Resolution belongs to a explicit
        // migration, never to a draft apply.
        return reject(view.reasonCode);
    }

    const ColorAssignmentApplyResult applied =
        applyColorAssignmentDraft(view.document, draft);
    if (!applied.ok) {
        return reject(QStringLiteral("draft-apply/") + applied.reasonCode);
    }
    const std::optional<QVariant> encoded = encodeAssignmentDocument(applied.next);
    if (!encoded.has_value()) {
        return reject(QStringLiteral("draft-apply/encode-failed"));
    }

    // AGENT-GUARD: Arm store state before crossing the injected client seam.
    // A legal fake transport can complete synchronously inside setUserValue;
    // arming afterward strands the store forever (P2.2).
    m_expectedDocument = applied.next;
    m_applyInFlight = true;
    if (!m_client.setUserValue(QLatin1String(ColorAssignmentsSettingsKey), *encoded, error)) {
        m_applyInFlight = false;
        m_expectedDocument.reset();
        if (error != nullptr && error->isEmpty()) {
            *error = QStringLiteral("transport-rejected");
        }
        return false;
    }
    return true;
}

void SettingsAssignmentStore::handleCommitFinished(const CommitOutcome &outcome)
{
    if (!m_applyInFlight) {
        // AGENT-GUARD: The store owns exactly one client and one write
        // lineage; a reply without a pending apply would mean a foreign
        // writer on this client and is ignored fail-closed.
        return;
    }
    m_applyInFlight = false;
    const std::optional<AssignmentDocument> expected = std::move(m_expectedDocument);
    m_expectedDocument.reset();

    AssignmentApplyOutcome result;
    result.revisionAfter = outcome.revisionAfter;

    if (outcome.status == SettingsWireStatus::Applied) {
        const AssignmentDocumentDecodeResult decoded = decodeAssignmentDocument(
            outcome.currentValues.value(QLatin1String(ColorAssignmentsSettingsKey)));
        if (!decoded.ok) {
            result.status = ApplyStatus::Uncertain;
            result.reasonCode = QStringLiteral("applied-truth-unreadable");
        } else if (!expected.has_value() || decoded.document != *expected) {
            // AGENT-GUARD: A wire-level Applied status is insufficient. The
            // authoritative document must equal the exact merged value this
            // store submitted, or persistence truth is unproven (P1.4).
            result.status = ApplyStatus::Uncertain;
            result.reasonCode = QStringLiteral("applied-truth-mismatch");
        } else {
            result.persistedDocument = decoded.document;
            if (outcome.revisionAfter == outcome.revisionBefore &&
                outcome.changedKeys.isEmpty()) {
                result.status = ApplyStatus::AppliedNoOp;
            } else {
                result.status = ApplyStatus::Applied;
            }
        }
    } else if (outcome.status == SettingsWireStatus::Conflict) {
        result.status = ApplyStatus::Conflict;
        result.reasonCode = QStringLiteral("revision-conflict");
    } else {
        result.status = ApplyStatus::Failed;
        result.reasonCode = wireStatusName(outcome.status);
    }

    Q_EMIT applyFinished(result);
}

} // namespace QindaQt::DisplayColor
