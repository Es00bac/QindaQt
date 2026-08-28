// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::Services::ClipboardModel {

// Fixed protocol bounds for the clipboard model. They are the ceiling for
// every history instance and every encoded form of a value or descriptor; a
// runtime HistoryLimits may only narrow them, never widen them. The future
// Wayland/Clipboard1 adapter must reuse these constants instead of restating
// its own numbers.
inline constexpr int kMaxEntries = 64;
inline constexpr int kMaxPinnedEntries = 8;
inline constexpr int kMaxFormatsPerItem = 8;
inline constexpr int kMaxMediaTypeLength = 127;
inline constexpr int kMaxSourceLabelCodeUnits = 64;
inline constexpr int kMaxPreviewCodeUnits = 96;
inline constexpr qint64 kMaxItemPayloadBytes = 1024 * 1024;
inline constexpr qint64 kMaxTotalPayloadBytes = 8 * 1024 * 1024;

enum class ClipboardError {
    None,
    EmptyValue,
    DuplicateFormat,
    MediaTypeRejected,
    SensitiveRefused,
    OneTimeRefused,
    NonStorableRefused,
    OversizedValue,
    TooManyFormats,
    HistoryDisabled,
    PrivacyDenied,
    StaleGeneration,
    UnknownEntry,
    PinnedLimitReached,
    CapacityRefused,
    MalformedData,
    UnsupportedVersion,
};

// AGENT-CONTRACT: classification is an allowlist. Anything not listed as
// storable in clipboard_media.cpp is NonStorable, so an unknown future MIME
// type can never enter the volatile history by default. Sensitive outranks
// one-time, which outranks non-storable when an item mixes classes.
enum class MediaClass {
    Storable,
    NonStorable,
    Sensitive,
    OneTime,
};

// Fail-closed default. Only an authenticated owner (the future host adapter
// driving this model) may set Allowed, and the transition back to Denied
// always purges the history and raises the generation.
enum class PrivacyState {
    Denied,
    Allowed,
};

enum class ClearScope {
    UnpinnedOnly,
    All,
};

// A limits instance may only narrow the fixed protocol bounds; this is the
// single validity rule shared by the model constructor and codecs.
struct HistoryLimits;
[[nodiscard]] bool isValidLimits(const HistoryLimits &limits) noexcept;

struct HistoryLimits {
    int maxEntries = kMaxEntries;
    int maxPinnedEntries = kMaxPinnedEntries;
    int maxFormatsPerItem = kMaxFormatsPerItem;
    int maxMediaTypeLength = kMaxMediaTypeLength;
    int maxSourceLabelCodeUnits = kMaxSourceLabelCodeUnits;
    int maxPreviewCodeUnits = kMaxPreviewCodeUnits;
    qint64 maxItemPayloadBytes = kMaxItemPayloadBytes;
    qint64 maxTotalPayloadBytes = kMaxTotalPayloadBytes;

    [[nodiscard]] bool isValid() const noexcept { return isValidLimits(*this); }
    friend bool operator==(const HistoryLimits &, const HistoryLimits &) = default;
};

// Generation-tagged identifier. Entries admitted under an earlier generation
// can never be resolved after a purge even if serial numbers repeat, which is
// what makes stale handles fail closed instead of addressing recycled state.
struct EntryId {
    quint32 generation = 0;
    quint32 serial = 0;

    [[nodiscard]] bool isValid() const noexcept { return generation != 0 && serial != 0; }
    friend bool operator==(const EntryId &, const EntryId &) = default;
};

struct ClipboardFormat {
    // Always canonical once inside the model; raw producer spellings are
    // canonicalized during admission, never stored as-is.
    QString mediaType;
    QByteArray payload;

    friend bool operator==(const ClipboardFormat &, const ClipboardFormat &) = default;
};

struct ClipboardValue {
    QList<ClipboardFormat> formats;

    friend bool operator==(const ClipboardValue &, const ClipboardValue &) = default;
};

struct FormatInfo {
    QString mediaType;
    qint64 payloadBytes = 0;

    friend bool operator==(const FormatInfo &, const FormatInfo &) = default;
};

// Metadata-only projection of one history entry. It deliberately carries no
// payload bytes: snapshots and descriptors can be handed to presentation and
// future wire transports without exposing content. Payload bytes leave the
// owning model only through an explicit promote() (the model-level analogue
// of a user-initiated paste).
struct ClipboardEntryDescriptor {
    EntryId id;
    quint64 admittedTick = 0;
    quint64 lastUsedTick = 0;
    bool pinned = false;
    QString sourceLabel;
    QString preview;
    bool previewTruncated = false;
    // SHA-256 over the canonical format list and payloads; used for dedup
    // and safe equality checks without comparing payloads directly.
    QByteArray fingerprint;
    QList<FormatInfo> formats;

    friend bool operator==(const ClipboardEntryDescriptor &, const ClipboardEntryDescriptor &) = default;
};

struct HistorySnapshot {
    quint32 generation = 0;
    quint64 revision = 0;
    bool historyEnabled = false;
    bool privacyAllowed = false;
    qint64 totalPayloadBytes = 0;
    // Most recent first. Empty whenever history is disabled or privacy is
    // denied; denial is also visible through privacyAllowed so presentation
    // can distinguish "nothing stored" from "withheld".
    QList<ClipboardEntryDescriptor> entries;

    friend bool operator==(const HistorySnapshot &, const HistorySnapshot &) = default;
};

// Canonicalizes every format of a value against instance limits and reports
// the first deterministic refusal. On acceptance, canonicalFormats is the
// ordered list admission stores, and totalPayloadBytes is the sum every
// capacity decision must use.
struct ValueValidation {
    ClipboardError error = ClipboardError::None;
    QString offendingMediaType;
    QList<ClipboardFormat> canonicalFormats;
    qint64 totalPayloadBytes = 0;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

struct AdmitOutcome {
    ClipboardError error = ClipboardError::None;
    ClipboardEntryDescriptor entry;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

struct PromoteOutcome {
    ClipboardError error = ClipboardError::None;
    ClipboardValue value;
    ClipboardEntryDescriptor entry;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

struct MutationOutcome {
    ClipboardError error = ClipboardError::None;

    [[nodiscard]] bool accepted() const noexcept { return error == ClipboardError::None; }
};

} // namespace QindaQt::Services::ClipboardModel

Q_DECLARE_METATYPE(QindaQt::Services::ClipboardModel::EntryId)
Q_DECLARE_METATYPE(QindaQt::Services::ClipboardModel::ClipboardEntryDescriptor)
