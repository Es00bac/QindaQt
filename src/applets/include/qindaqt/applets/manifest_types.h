// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <QString>

#include <optional>

namespace QindaQt::Applets {

enum class EntryPointKind {
    Builtin,
    Qml,
    Executable,
};

enum class PlacementZone {
    PanelStart,
    PanelCenter,
    PanelEnd,
    PanelFill,
    Desktop,
};

enum class Orientation {
    Horizontal,
    Vertical,
};

enum class Capability {
    ApplicationLaunch,
    WindowRead,
    WindowActivate,
    WindowManage,
    GlobalMenuRead,
    StatusItemRead,
    StatusItemActivate,
    NotificationRead,
    AudioRead,
    AudioControl,
    PowerRead,
    ClipboardRead,
    ClipboardWrite,
    BluetoothRead,
    BluetoothControl,
    DisplayRead,
    DisplayControl,
    SettingsRead,
};

struct EntryPoint final {
    EntryPointKind kind = EntryPointKind::Builtin;
    QString value;

    bool operator==(const EntryPoint &) const = default;
};

struct AxisSizing final {
    int minimum = 0;
    int preferred = 0;
    std::optional<int> maximum;
    bool stretch = false;

    bool operator==(const AxisSizing &) const = default;
};

struct SizingConstraints final {
    AxisSizing mainAxis;
    AxisSizing crossAxis;

    bool operator==(const SizingConstraints &) const = default;
};

[[nodiscard]] QString toString(EntryPointKind value);
[[nodiscard]] QString toString(PlacementZone value);
[[nodiscard]] QString toString(Orientation value);
[[nodiscard]] QString toString(Capability value);

[[nodiscard]] std::optional<EntryPointKind> entryPointKindFromString(const QString &value);
[[nodiscard]] std::optional<PlacementZone> placementZoneFromString(const QString &value);
[[nodiscard]] std::optional<Orientation> orientationFromString(const QString &value);
[[nodiscard]] std::optional<Capability> capabilityFromString(const QString &value);

} // namespace QindaQt::Applets
