// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::StatusNotifierApplet {

// Bounded presentation limits for the status-notifier tray applet surface.
// AGENT-CONTRACT: this cap sits BELOW the S1 registry's kMaxItems (64); the
// projection truncates truthfully (overflowCount/overflowText) instead of
// growing the presented surface without bound.
inline constexpr qsizetype kMaxPresentedItems = 24;

enum class AppletPhase {
    Loading,
    Ready,
    Empty,
    Degraded,
    Unavailable,
};

[[nodiscard]] QString phaseToString(AppletPhase phase) noexcept;

// Projection of one tray item row tailored for QML consumption.
// AGENT-CONTRACT: metadata only — no D-Bus handles, payload pointers, or IPC
// endpoints cross into QML. The icon crosses as a bounded PNG data URL the
// controller renders through the injected seam; `iconDataUrl` stays empty and
// `iconIsPlaceholder` false until that enrichment runs.
struct StatusNotifierItemRow {
    Q_GADGET
    Q_PROPERTY(QString uniqueName MEMBER uniqueName CONSTANT)
    Q_PROPERTY(QString objectPath MEMBER objectPath CONSTANT)
    Q_PROPERTY(quint64 generation MEMBER generation CONSTANT)
    Q_PROPERTY(QString identity MEMBER identity CONSTANT)
    Q_PROPERTY(QString title MEMBER title CONSTANT)
    Q_PROPERTY(QString accessibleName MEMBER accessibleName CONSTANT)
    Q_PROPERTY(QString accessibleDescription MEMBER accessibleDescription CONSTANT)
    Q_PROPERTY(QString accessibleStatusText MEMBER accessibleStatusText CONSTANT)
    Q_PROPERTY(bool needsAttention MEMBER needsAttention CONSTANT)
    Q_PROPERTY(bool active MEMBER active CONSTANT)
    Q_PROPERTY(bool hasMenu MEMBER hasMenu CONSTANT)
    Q_PROPERTY(int menuEntryCount MEMBER menuEntryCount CONSTANT)
    Q_PROPERTY(QString keyboardActivateText MEMBER keyboardActivateText CONSTANT)
    Q_PROPERTY(QString keyboardContextMenuText MEMBER keyboardContextMenuText CONSTANT)
    Q_PROPERTY(bool secondaryActivatePointerOnly MEMBER secondaryActivatePointerOnly CONSTANT)
    Q_PROPERTY(QString iconDataUrl MEMBER iconDataUrl CONSTANT)
    Q_PROPERTY(bool iconIsPlaceholder MEMBER iconIsPlaceholder CONSTANT)

public:
    QString uniqueName;
    QString objectPath;
    quint64 generation = 0;
    QString identity;
    QString title;
    QString accessibleName;
    QString accessibleDescription;
    QString accessibleStatusText;
    bool needsAttention = false;
    bool active = false;
    bool hasMenu = false;
    int menuEntryCount = 0;
    QString keyboardActivateText;
    QString keyboardContextMenuText;
    // AGENT-NOTE: S1 records secondary activation as pointer-only (its
    // PresentationTexts.keyboardSecondaryActivate stays empty by design), so
    // this is always true with the S1 texts; the row states it truthfully
    // instead of inventing a keyboard route.
    bool secondaryActivatePointerOnly = true;
    QString iconDataUrl;
    bool iconIsPlaceholder = false;

    friend bool operator==(const StatusNotifierItemRow &, const StatusNotifierItemRow &) = default;
};

// One flattened, depth-capped menu row for the read-only context preview.
struct StatusNotifierMenuRow {
    Q_GADGET
    Q_PROPERTY(int depth MEMBER depth CONSTANT)
    Q_PROPERTY(QString kind MEMBER kind CONSTANT)
    Q_PROPERTY(QString label MEMBER label CONSTANT)
    Q_PROPERTY(bool enabled MEMBER enabled CONSTANT)
    Q_PROPERTY(bool visible MEMBER visible CONSTANT)
    Q_PROPERTY(bool hasChildren MEMBER hasChildren CONSTANT)

public:
    int depth = 0;
    // One of "item", "separator", "submenu".
    QString kind;
    QString label;
    bool enabled = true;
    bool visible = true;
    bool hasChildren = false;

    friend bool operator==(const StatusNotifierMenuRow &, const StatusNotifierMenuRow &) = default;
};

struct StatusNotifierAppletProjection {
    AppletPhase phase = AppletPhase::Loading;
    // Reason-code string: the S1 diagnostic for Degraded, the registered
    // refusal code for Unavailable, empty otherwise.
    QString phaseReason;
    QList<StatusNotifierItemRow> rows;
    int presentedCount = 0;
    int overflowCount = 0;
    QString overflowText;

    friend bool operator==(const StatusNotifierAppletProjection &,
                           const StatusNotifierAppletProjection &) = default;
};

// AGENT-CONTRACT: Localization boundary for applet-level strings that the S1
// PresentationTexts does not own (overflow naming, capability-denial and
// intent-refusal feedback). The shell presenter supplies locale-appropriate
// values; the defaults are deterministic fallbacks for tests and early
// integration.
struct StatusNotifierAppletTexts {
    QString overflowMoreItems = QStringLiteral("%1 more items");
    QString overflowMoreItem = QStringLiteral("%1 more item");
    QString emptyText = QStringLiteral("No status items.");
    QString unavailableReadNotGranted =
        QStringLiteral("Status items are not shown because the status-items read permission was not granted.");
    QString feedbackStaleItem =
        QStringLiteral("This status item is no longer available.");
    QString feedbackRefused =
        QStringLiteral("The status item refused the request (%1).");
    QString feedbackActivateNotGranted =
        QStringLiteral("Activating status items is not permitted.");
    QString feedbackBusy =
        QStringLiteral("Another status item request is already being sent.");

    friend bool operator==(const StatusNotifierAppletTexts &,
                           const StatusNotifierAppletTexts &) = default;
};

// AGENT-CONTRACT: registered reason codes for the Unavailable phase;
// QML/tests match on these exact tokens.
inline constexpr char kReasonStatusItemsReadNotGranted[] = "status-items-read-not-granted";
inline constexpr char kReasonSourceUnavailable[] = "status-notifier-source-unavailable";

} // namespace QindaQt::StatusNotifierApplet

Q_DECLARE_METATYPE(QindaQt::StatusNotifierApplet::AppletPhase)
Q_DECLARE_METATYPE(QindaQt::StatusNotifierApplet::StatusNotifierItemRow)
Q_DECLARE_METATYPE(QindaQt::StatusNotifierApplet::StatusNotifierMenuRow)
Q_DECLARE_METATYPE(QindaQt::StatusNotifierApplet::StatusNotifierAppletProjection)
