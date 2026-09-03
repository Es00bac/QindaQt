// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/display_color_model/color_limits.h>
#include <qindaqt/services/display_color_model/color_validation.h>

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QVariant>

#include <optional>

namespace QindaQt::DisplayColor
{

// AGENT-CONTRACT: The persisted Settings1 value of the documented key
// "displays.colorAssignments" maps each output stable ID to exactly one
// assignment record. The canonical on-disk/wire shape is
//   { "<output stable id>": { "profile": "<profile id>",
//                             "lineage": "<64 lowercase hex or empty>" } }
// Decoding is all-or-nothing (any hostile field rejects the whole document),
// the number of outputs is capped by MaxOutputs, and profile identifiers use
// the C0 identifier grammar. The lineage fingerprint is the raw SHA-256
// content digest of an imported profile (32 bytes, or empty when the
// assignment predates a verified import); on the wire it is lowercase hex.

struct ColorAssignmentRecord
{
    QString outputStableId;
    QString profileId;
    QByteArray lineageFingerprint; // raw 32 bytes, or empty

    friend bool operator==(const ColorAssignmentRecord &, const ColorAssignmentRecord &) = default;
};

struct AssignmentDocument
{
    // Deterministically ordered by output stable ID.
    QList<ColorAssignmentRecord> records;

    friend bool operator==(const AssignmentDocument &, const AssignmentDocument &) = default;
};

struct AssignmentDocumentDecodeResult
{
    bool ok = false;
    QString reasonCode;
    AssignmentDocument document;
};

// Strict canonical decode of a Settings1 value into typed records. Accepts
// only the exact documented shape: a bounded map of valid stable IDs to
// objects with exactly the fields "profile" (valid identifier string) and
// "lineage" (empty or exactly 64 lowercase hex characters). Any other shape,
// type, field set, grammar violation, or output over the C0 aggregate cap
// rejects the complete document with a stable reason code.
AssignmentDocumentDecodeResult decodeAssignmentDocument(const QVariant &value);

// Canonical encode. QVariantMap keys iterate in sorted order, so the
// produced value is byte-stable for equal documents. Returns nullopt when
// any record fails validation — encoding never silently repairs.
std::optional<QVariant> encodeAssignmentDocument(const AssignmentDocument &document);

struct ColorAssignmentDraftEntry
{
    QString outputStableId;
    QString profileId; // ignored when remove is true
    QByteArray lineageFingerprint;
    bool remove = false;

    friend bool operator==(const ColorAssignmentDraftEntry &,
                           const ColorAssignmentDraftEntry &) = default;
};

struct ColorAssignmentDraft
{
    QList<ColorAssignmentDraftEntry> entries;

    friend bool operator==(const ColorAssignmentDraft &, const ColorAssignmentDraft &) = default;
};

struct ColorAssignmentDraftValidation
{
    bool ok = false;
    QString reasonCode;
};

// Validates a draft: every entry carries a valid stable ID, assignments name
// a valid profile ID with an empty or 32-byte lineage fingerprint, the same
// output is not targeted twice, and the distinct-output count stays within
// the C0 aggregate cap.
ColorAssignmentDraftValidation validateColorAssignmentDraft(const ColorAssignmentDraft &draft);

struct ColorAssignmentApplyResult
{
    bool ok = false;
    QString reasonCode;
    AssignmentDocument next;
};

// Pure draft application on top of a decoded document. Removes drop the
// output's record; assignments replace it. The input document is never
// mutated; an invalid draft leaves the caller free to discard the result.
ColorAssignmentApplyResult applyColorAssignmentDraft(const AssignmentDocument &document,
                                                     const ColorAssignmentDraft &draft);

// The documented Settings1 key that carries this document.
inline constexpr auto ColorAssignmentsSettingsKey = "displays.colorAssignments";

// SHA-256 sizing shared by lineage fingerprints (raw digest bytes and the
// canonical lowercase hex wire form).
inline constexpr int Sha256DigestBytes = 32;
inline constexpr int Sha256HexLength = 64;

} // namespace QindaQt::DisplayColor
