// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/profiles/layout_profile.h"
#include "qindaqt/profiles/profile_types.h"

#include <QString>
#include <QtTypes>

#include <optional>
#include <variant>

namespace QindaQt::ShellCustomizationEditor {

// Drag payload: what the user picked up. A palette drag creates a new applet
// instance; an instance drag moves or copies an existing profile-global applet
// (ADR-0006). Identity is captured once at arm time and never re-derived.
enum class PayloadKind {
    PalettePlugin,
    AppletInstance,
};

struct DragPayload final {
    PayloadKind kind = PayloadKind::PalettePlugin;
    QString pluginId;
    QString sourcePanelId;
    QString sourceAppletId;

    [[nodiscard]] bool isPalette() const noexcept
    {
        return kind == PayloadKind::PalettePlugin;
    }

    bool operator==(const DragPayload &) const = default;
};

// Resolved drop target identity. One gesture equals one target identity:
// in-drag commands execute only when this value changes (architecture D3).
// beforeAppletId is the flat-list anchor inside the target panel; an empty
// value appends at the panel end.
struct DropTarget final {
    QString panelId;
    QString zone = QStringLiteral("start");
    std::optional<QString> beforeAppletId;

    bool operator==(const DropTarget &) const = default;
};

// The editor offers only zones the v1 runtime honors (start/center/end);
// `panel-fill` and `desktop` must not be offered before their schema slices
// land (architecture D25/D9). Structural validation rejects them before any
// command is built.
[[nodiscard]] bool isValidEditorZone(const QString &zone);

enum class IntentKind {
    InsertApplet,
    MoveApplet,
    RemoveApplet,
    DuplicateApplet,
    ConfigurePanel,
    MovePanel,
};

struct InsertAppletIntent final {
    QString pluginId;
};

struct MoveAppletIntent final {
    QString panelId;
    QString appletId;
};

struct RemoveAppletIntent final {
    QString panelId;
    QString appletId;
};

struct DuplicateAppletIntent final {
    QString panelId;
    QString appletId;
    QString newAppletId;
};

// ConfigurePanelCommand replaces all five fields at once, so the intent always
// carries the complete current tuple with one field changed by the caller.
struct PanelConfiguration final {
    Profiles::Layer layer = Profiles::Layer::Above;
    Profiles::HideMode hideMode = Profiles::HideMode::Never;
    int rows = 1;
    int thickness = 32;
    double length = 1.0;

    bool operator==(const PanelConfiguration &) const = default;
};

struct ConfigurePanelIntent final {
    QString panelId;
    PanelConfiguration configuration;
};

struct MovePanelIntent final {
    QString panelId;
    QString outputId;
    Profiles::Edge edge = Profiles::Edge::Top;
    Profiles::Alignment alignment = Profiles::Alignment::Fill;
    std::optional<QString> beforePanelId;
};

using CustomizationIntent = std::variant<InsertAppletIntent,
                                         MoveAppletIntent,
                                         RemoveAppletIntent,
                                         DuplicateAppletIntent,
                                         ConfigurePanelIntent,
                                         MovePanelIntent>;

[[nodiscard]] IntentKind intentKind(const CustomizationIntent &intent) noexcept;

// Intent factories keep presentation code free of variant construction.
[[nodiscard]] CustomizationIntent paletteInsertIntent(const DragPayload &payload);
[[nodiscard]] CustomizationIntent instanceMoveIntent(const DragPayload &payload);
[[nodiscard]] CustomizationIntent removeIntent(const QString &panelId, const QString &appletId);
[[nodiscard]] CustomizationIntent duplicateIntent(const QString &panelId,
                                                  const QString &appletId,
                                                  const QString &newAppletId);
[[nodiscard]] CustomizationIntent configureIntent(const QString &panelId,
                                                  const PanelConfiguration &configuration);
[[nodiscard]] CustomizationIntent movePanelIntent(const QString &panelId,
                                                  const QString &outputId,
                                                  Profiles::Edge edge,
                                                  Profiles::Alignment alignment,
                                                  const std::optional<QString> &beforePanelId);

// Structural validation only. Existence, manifest compatibility, collision,
// and layout acceptance are engine authority (evaluate()/execute()); this pass
// must never duplicate that policy.
enum class IntentErrorCode {
    None,
    EmptyPluginId,
    EmptyPanelId,
    EmptyAppletId,
    EmptyNewAppletId,
    InvalidZone,
    AnchorSelfReference,
    InvalidConfiguration,
};

struct IntentValidation final {
    IntentErrorCode code = IntentErrorCode::None;
    QString message;

    [[nodiscard]] bool ok() const noexcept { return code == IntentErrorCode::None; }
};

[[nodiscard]] IntentValidation validateIntent(const CustomizationIntent &intent,
                                              const DropTarget &target);

} // namespace QindaQt::ShellCustomizationEditor
