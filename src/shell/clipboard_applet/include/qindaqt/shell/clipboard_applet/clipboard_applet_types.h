// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>
#include <qindaqt/services/clipboard_model/clipboard_types.h>

namespace QindaQt::ShellClipboardApplet {

// Bounded presentation limits for the clipboard applet surface.
inline constexpr int kMaxPresentedEntries = 32;
inline constexpr int kMaxSearchQueryLength = QindaQt::Services::ClipboardModel::kMaxPreviewCodeUnits;

enum class ClientState {
    Stopped,
    Starting,
    Ready,
    Unavailable,
    Degraded,
};

enum class Phase {
    Loading,
    Ready,
    Degraded,
    Unavailable,
    Locked,
    Disabled,
};

[[nodiscard]] QString phaseToString(Phase phase) noexcept;
[[nodiscard]] Phase phaseFromString(const QString &phaseText) noexcept;

enum class OperationKind {
    Promote,
    Remove,
    SetPinned,
    Clear,
    Search,
};

enum class OperationErrorCode {
    None,
    HistoryDisabled,
    PrivacyDenied,
    Locked,
    StaleGeneration,
    OwnerLost,
    UnknownEntry,
    PinnedLimitReached,
    CapacityRefused,
    InvalidQuery,
    Busy,
    Failed,
};

struct OperationOutcome {
    OperationErrorCode code = OperationErrorCode::None;
    QString message;
    QindaQt::Services::ClipboardModel::EntryId id;

    [[nodiscard]] bool ok() const noexcept { return code == OperationErrorCode::None; }
    friend bool operator==(const OperationOutcome &, const OperationOutcome &) = default;
};

// Projection of a single history entry row tailored for QML consumption.
// AGENT-CONTRACT: This structure contains only metadata and bounded preview text.
// Raw payload bytes are never held here, maintaining memory safety and the
// metadata-only projection invariant.
struct ClipboardEntryRow {
    Q_GADGET
    Q_PROPERTY(quint32 generation MEMBER generation CONSTANT)
    Q_PROPERTY(quint32 serial MEMBER serial CONSTANT)
    Q_PROPERTY(QString idString MEMBER idString CONSTANT)
    Q_PROPERTY(QString preview MEMBER preview CONSTANT)
    Q_PROPERTY(bool previewTruncated MEMBER previewTruncated CONSTANT)
    Q_PROPERTY(QString sourceLabel MEMBER sourceLabel CONSTANT)
    Q_PROPERTY(bool pinned MEMBER pinned CONSTANT)
    Q_PROPERTY(QString formatsSummary MEMBER formatsSummary CONSTANT)
    Q_PROPERTY(QString primaryMediaType MEMBER primaryMediaType CONSTANT)
    Q_PROPERTY(bool isText MEMBER isText CONSTANT)
    Q_PROPERTY(bool isImage MEMBER isImage CONSTANT)
    Q_PROPERTY(bool isUriList MEMBER isUriList CONSTANT)
    Q_PROPERTY(qint64 totalBytes MEMBER totalBytes CONSTANT)
    Q_PROPERTY(quint64 admittedTick MEMBER admittedTick CONSTANT)
    Q_PROPERTY(quint64 lastUsedTick MEMBER lastUsedTick CONSTANT)
    Q_PROPERTY(QString accessibleName MEMBER accessibleName CONSTANT)
    Q_PROPERTY(QString accessibleDescription MEMBER accessibleDescription CONSTANT)
    Q_PROPERTY(bool pending MEMBER pending CONSTANT)

public:
    quint32 generation = 0;
    quint32 serial = 0;
    QString idString;
    QString preview;
    bool previewTruncated = false;
    QString sourceLabel;
    bool pinned = false;
    QString formatsSummary;
    QString primaryMediaType;
    bool isText = false;
    bool isImage = false;
    bool isUriList = false;
    qint64 totalBytes = 0;
    quint64 admittedTick = 0;
    quint64 lastUsedTick = 0;
    QString accessibleName;
    QString accessibleDescription;
    bool pending = false;

    friend bool operator==(const ClipboardEntryRow &, const ClipboardEntryRow &) = default;
};

struct ClipboardAppletProjection {
    Phase phase = Phase::Loading;
    QString phaseReasonText;
    QList<ClipboardEntryRow> entryRows;
    int pinnedCount = 0;
    int unpinnedCount = 0;
    qint64 totalPayloadBytes = 0;
    QString totalPayloadBytesFormatted;
    bool isSearchActive = false;
    QString searchQuery;
    int searchResultCount = 0;
    bool searchTruncated = false;
    QString emptyReasonText;

    friend bool operator==(const ClipboardAppletProjection &, const ClipboardAppletProjection &) = default;
};

} // namespace QindaQt::ShellClipboardApplet

Q_DECLARE_METATYPE(QindaQt::ShellClipboardApplet::ClientState)
Q_DECLARE_METATYPE(QindaQt::ShellClipboardApplet::Phase)
Q_DECLARE_METATYPE(QindaQt::ShellClipboardApplet::ClipboardEntryRow)
Q_DECLARE_METATYPE(QindaQt::ShellClipboardApplet::ClipboardAppletProjection)
