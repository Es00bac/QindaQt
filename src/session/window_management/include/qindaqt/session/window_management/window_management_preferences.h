// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace QindaQt::Session::WindowManagement {

enum class FocusPolicy { Click, FocusFollowsMouse, FocusUnderMouse };
enum class DockingModifier { Super, Alt, Control, Disabled };
enum class CloseContainerPolicy { Ask, CloseAll, Ungroup };

// AGENT-CONTRACT: the complete Settings1 `windowManagement.*` scope, decoded
// totally: a snapshot that lacks a key or carries a value outside the schema
// fails as a whole and the bridge keeps the last good preferences. Every key
// below is declared in data/settings/schema-v2.json.
struct WindowManagementPreferences final {
    FocusPolicy focusPolicy = FocusPolicy::Click;
    DockingModifier dockingModifier = DockingModifier::Super;
    int snapDistance = 12;
    bool sessionRestore = true;
    CloseContainerPolicy closeContainerPolicy = CloseContainerPolicy::Ask;

    [[nodiscard]] bool operator==(const WindowManagementPreferences &) const = default;

    [[nodiscard]] static QStringList scopedKeys();
    [[nodiscard]] static std::optional<WindowManagementPreferences> fromVariantMap(
        const QVariantMap &values, QString *error = nullptr);

    // The exact kwinrc spellings the bridge writes and the compositor reads.
    [[nodiscard]] static QString kwinFocusPolicy(FocusPolicy policy);
    [[nodiscard]] static QString dockingModifierName(DockingModifier modifier);
    [[nodiscard]] static QString closeContainerPolicyName(CloseContainerPolicy policy);
};

} // namespace QindaQt::Session::WindowManagement
