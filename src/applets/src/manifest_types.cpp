// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applets/manifest_types.h"

#include <array>
#include <utility>

namespace QindaQt::Applets {
namespace {

template<typename Enum>
struct EnumToken final {
    Enum value;
    const char *token;
};

constexpr std::array entryPointKinds{
    EnumToken{EntryPointKind::Builtin, "builtin"},
    EnumToken{EntryPointKind::Qml, "qml"},
    EnumToken{EntryPointKind::Executable, "executable"},
};

constexpr std::array placementZones{
    EnumToken{PlacementZone::PanelStart, "panel-start"},
    EnumToken{PlacementZone::PanelCenter, "panel-center"},
    EnumToken{PlacementZone::PanelEnd, "panel-end"},
    EnumToken{PlacementZone::PanelFill, "panel-fill"},
    EnumToken{PlacementZone::Desktop, "desktop"},
};

constexpr std::array orientations{
    EnumToken{Orientation::Horizontal, "horizontal"},
    EnumToken{Orientation::Vertical, "vertical"},
};

constexpr std::array capabilities{
    EnumToken{Capability::ApplicationLaunch, "applications.launch"},
    EnumToken{Capability::WindowRead, "windows.read"},
    EnumToken{Capability::WindowActivate, "windows.activate"},
    EnumToken{Capability::WindowManage, "windows.manage"},
    EnumToken{Capability::GlobalMenuRead, "global-menu.read"},
    EnumToken{Capability::StatusItemRead, "status-items.read"},
    EnumToken{Capability::StatusItemActivate, "status-items.activate"},
    EnumToken{Capability::NotificationRead, "notifications.read"},
    EnumToken{Capability::AudioRead, "audio.read"},
    EnumToken{Capability::AudioControl, "audio.control"},
    EnumToken{Capability::PowerRead, "power.read"},
    EnumToken{Capability::ClipboardRead, "clipboard.read"},
    EnumToken{Capability::ClipboardWrite, "clipboard.write"},
    EnumToken{Capability::BluetoothRead, "bluetooth.read"},
    EnumToken{Capability::BluetoothControl, "bluetooth.control"},
    EnumToken{Capability::DisplayRead, "display.read"},
    EnumToken{Capability::DisplayControl, "display.control"},
    EnumToken{Capability::SettingsRead, "settings.read"},
};

template<typename Enum, std::size_t Size>
QString serializeEnum(Enum value, const std::array<EnumToken<Enum>, Size> &mapping)
{
    for (const auto &entry : mapping) {
        if (entry.value == value) {
            return QString::fromLatin1(entry.token);
        }
    }
    return {};
}

template<typename Enum, std::size_t Size>
std::optional<Enum> parseEnum(const QString &value,
                              const std::array<EnumToken<Enum>, Size> &mapping)
{
    for (const auto &entry : mapping) {
        if (value == QLatin1String(entry.token)) {
            return entry.value;
        }
    }
    return std::nullopt;
}

} // namespace

QString toString(EntryPointKind value)
{
    return serializeEnum(value, entryPointKinds);
}

QString toString(PlacementZone value)
{
    return serializeEnum(value, placementZones);
}

QString toString(Orientation value)
{
    return serializeEnum(value, orientations);
}

QString toString(Capability value)
{
    return serializeEnum(value, capabilities);
}

std::optional<EntryPointKind> entryPointKindFromString(const QString &value)
{
    return parseEnum(value, entryPointKinds);
}

std::optional<PlacementZone> placementZoneFromString(const QString &value)
{
    return parseEnum(value, placementZones);
}

std::optional<Orientation> orientationFromString(const QString &value)
{
    return parseEnum(value, orientations);
}

std::optional<Capability> capabilityFromString(const QString &value)
{
    return parseEnum(value, capabilities);
}

} // namespace QindaQt::Applets
