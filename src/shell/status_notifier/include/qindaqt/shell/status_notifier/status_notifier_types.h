// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QString>

namespace QindaQt::StatusNotifier
{

enum class ItemCategory : quint32 {
    Application = 0,
    Communications = 1,
    SystemServices = 2,
    Hardware = 3,
};

enum class ItemStatus : quint32 {
    Passive = 0,
    Active = 1,
    NeedsAttention = 2,
};

// The three user-visible item operations the tray can intend. S0 records and
// validates these intents only; nothing in this module executes them.
enum class RequestKind : quint32 {
    Activate = 0,
    ContextMenu = 1,
    SecondaryActivate = 2,
};

enum class PresentationState : quint32 {
    Loading = 0,
    Ready = 1,
    Empty = 2,
    Degraded = 3,
};

enum class RegistryStatus : quint32 {
    Accepted = 0,
    InvalidOwner = 1,
    InvalidDescriptor = 2,
    UnknownItem = 3,
    StaleOwner = 4,
    DuplicateIdentity = 5,
    CapacityExceeded = 6,
    InvalidRequest = 7,
};

struct RegistryOutcome {
    RegistryStatus status = RegistryStatus::Accepted;
    QString reasonCode;

    [[nodiscard]] bool accepted() const noexcept { return status == RegistryStatus::Accepted; }

    friend bool operator==(const RegistryOutcome &, const RegistryOutcome &) = default;
};

// AGENT-NOTE: The exact owner of a tray item is its bus unique name, never a
// well-known name. Well-known names can change owners while items remain
// registered, so keying on them would let one source impersonate another.
// `generation` is the registry-issued owner generation, drawn from a
// globally monotonic counter so a value can never be reissued after owner loss,
// slot reuse, or wrap refusal; events stamped with an older generation are
// stale and must be rejected by the registry.
struct OwnerKey {
    QString uniqueName;
    QString objectPath;
    quint64 generation = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return generation != 0 && !uniqueName.isEmpty() && !objectPath.isEmpty();
    }

    friend bool operator==(const OwnerKey &, const OwnerKey &) = default;
};

struct Pixmap {
    quint32 width = 0;
    quint32 height = 0;
    // Premultiplied ARGB32, exactly width * height * 4 bytes when valid.
    QByteArray argb;

    friend bool operator==(const Pixmap &, const Pixmap &) = default;
};

struct IconPayload {
    QString iconName;
    QList<Pixmap> pixmaps;
    QString attentionIconName;
    QList<Pixmap> attentionPixmaps;
    QString attentionMovieName;

    friend bool operator==(const IconPayload &, const IconPayload &) = default;
};

struct ToolTipPayload {
    QString iconName;
    QList<Pixmap> pixmaps;
    QString title;
    QString description;

    friend bool operator==(const ToolTipPayload &, const ToolTipPayload &) = default;
};

struct MenuEntry {
    enum class Kind : quint32 {
        Item = 0,
        Separator = 1,
        SubMenu = 2,
    };

    Kind kind = Kind::Item;
    // AGENT-NOTE: The menu is a flat DBusMenu-style list, not a recursive
    // tree. Recursive value types break Qt's QTypeTraits equality detection
    // once declared as metatypes, and flat entries with parent indices match
    // how a future DBusMenu adapter decodes the wire anyway. `-1` means top
    // level; any other value must name an earlier entry.
    qsizetype parentId = -1;
    QString label;
    QString iconName;
    QString shortcut;
    bool enabled = true;
    bool visible = true;

    friend bool operator==(const MenuEntry &, const MenuEntry &) = default;
};

struct MenuPayload {
    QList<MenuEntry> entries;

    friend bool operator==(const MenuPayload &, const MenuPayload &) = default;
};

struct ItemDescriptor {
    ItemCategory category = ItemCategory::Application;
    // AGENT-GUARD: `identity` is the user-visible item identity the registry
    // deduplicates across owners. Two live owners must never present the same
    // identity, or the tray would show two items claiming to be one app.
    QString identity;
    QString title;
    ItemStatus status = ItemStatus::Passive;
    IconPayload icon;
    ToolTipPayload toolTip;
    MenuPayload menu;

    friend bool operator==(const ItemDescriptor &, const ItemDescriptor &) = default;
};

struct KeyboardAction {
    RequestKind kind = RequestKind::Activate;
    // Human-readable standard keyboard route. Empty means the action has no
    // keyboard equivalent yet and is reachable through pointer intent only.
    QString keyboardDescription;

    friend bool operator==(const KeyboardAction &, const KeyboardAction &) = default;
};

struct TrayItemPresentation {
    OwnerKey owner;
    QString identity;
    QString accessibleName;
    QString accessibleDescription;
    QString accessibleStatusText;
    QList<KeyboardAction> keyboardActions;

    friend bool operator==(const TrayItemPresentation &, const TrayItemPresentation &) = default;
};

struct TrayPresentation {
    PresentationState state = PresentationState::Loading;
    QList<TrayItemPresentation> items;
    QString diagnostic;

    friend bool operator==(const TrayPresentation &, const TrayPresentation &) = default;
};

// AGENT-CONTRACT: A validated request intent. The registry produces this value
// only after confirming the target owner's generation is currently live and
// that the same owner still presents `identity`. It carries an explicit
// lifetime: it is valid only while that generation remains current and the
// target key still presents the same identity. An executor must call the
// registry's revalidateIntent() immediately before performing anything. The
// registry never performs an intent; mapping one onto the owner's own D-Bus
// object belongs to a later presenter milestone.
struct RequestIntent {
    OwnerKey target;
    QString identity;
    RequestKind kind = RequestKind::Activate;

    friend bool operator==(const RequestIntent &, const RequestIntent &) = default;
};

} // namespace QindaQt::StatusNotifier

Q_DECLARE_METATYPE(QindaQt::StatusNotifier::ItemCategory)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::ItemStatus)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::RequestKind)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::PresentationState)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::OwnerKey)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::Pixmap)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::IconPayload)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::ToolTipPayload)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::MenuEntry)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::MenuPayload)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::ItemDescriptor)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::KeyboardAction)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::TrayItemPresentation)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::TrayPresentation)
Q_DECLARE_METATYPE(QindaQt::StatusNotifier::RequestIntent)
