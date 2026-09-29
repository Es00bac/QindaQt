// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QLatin1StringView>

// The external names of qindaqt-kwin, QindaQt's compositor (ADR-0291).
//
// AGENT-CONTRACT: the qindaqt-kwin fork defines every name here (its
// qindaqt/README.md, hub qinda:~/git/qindaqt-kwin.git). Change one only
// together with a fork release, and spell these names nowhere else in
// container-wm: include this header.
namespace QindaQt::CompositorNames
{

// The compositor program the QindaQt session launches.
inline constexpr QLatin1StringView executable{"qindaqt-kwin"};

// Config files, relative to $XDG_CONFIG_HOME: QindaQt's own folder.
inline constexpr QLatin1StringView configDirectory{"qindaqt"};
inline constexpr QLatin1StringView configFile{"qindaqt/kwinrc"};
inline constexpr QLatin1StringView rulesFile{"qindaqt/kwinrulesrc"};
inline constexpr QLatin1StringView outputConfigFile{"qindaqt/kwinoutputconfig.json"};
inline constexpr QLatin1StringView inputConfigFile{"qindaqt/kwininputrc"};
inline constexpr QLatin1StringView keyboardConfigFile{"qindaqt/kwinxkbrc"};
// Plugin namespaces below the Qt plugin path.
inline constexpr QLatin1StringView pluginNamespace{"qindaqt-kwin/plugins"};
inline constexpr QLatin1StringView decorationNamespace{"qindaqt-kwin/decorations"};
// State file, relative to $XDG_STATE_HOME.
inline constexpr QLatin1StringView stateFile{"qindaqt/kwinstaterc"};

// D-Bus. The fork owns org.qindaqt.KWin; every object lives under
// /org/qindaqt/KWin.
inline constexpr QLatin1StringView service{"org.qindaqt.KWin"};
inline constexpr QLatin1StringView objectPath{"/org/qindaqt/KWin"};
inline constexpr QLatin1StringView interfaceName{"org.qindaqt.KWin"};
inline constexpr QLatin1StringView virtualDesktopsPath{"/org/qindaqt/KWin/VirtualDesktopManager"};
inline constexpr QLatin1StringView virtualDesktopsInterface{"org.qindaqt.KWin.VirtualDesktopManager"};
inline constexpr QLatin1StringView inputDevicesPath{"/org/qindaqt/KWin/InputDevice"};
inline constexpr QLatin1StringView inputDeviceManagerInterface{"org.qindaqt.KWin.InputDeviceManager"};
inline constexpr QLatin1StringView inputDeviceInterface{"org.qindaqt.KWin.InputDevice"};
inline constexpr QLatin1StringView nightLightService{"org.qindaqt.KWin.NightLight"};
inline constexpr QLatin1StringView nightLightPath{"/org/qindaqt/KWin/NightLight"};
inline constexpr QLatin1StringView nightLightInterface{"org.qindaqt.KWin.NightLight"};

// Native lock backend admits lock/recovery and reports physical protection;
// there is no public unlock or authentication-result method (ADR-0299).
inline constexpr QLatin1StringView nativeLockPath{"/org/qindaqt/KWin/NativeLock"};
inline constexpr QLatin1StringView nativeLockInterface{"org.qindaqt.KWin.NativeLock1"};

// AGENT-NOTE: carve-outs until PF21 of the Plasma-free plan. The fork keeps
// ScreenShot2 under its KDE names because the KDE portal and Spectacle call
// it; QindaQt's callers move together with the rename in PF21. (Scripting at
// /Scripting stays too, for Gabbee; container-wm itself does not call it.)
inline constexpr QLatin1StringView screenshotService{"org.kde.KWin.ScreenShot2"};
inline constexpr QLatin1StringView screenshotPath{"/org/kde/KWin/ScreenShot2"};
inline constexpr QLatin1StringView screenshotInterface{"org.kde.KWin.ScreenShot2"};

// Privileged-client keys in a desktop file (the fork also honours the
// X-KDE-* keys until PF21).
inline constexpr QLatin1StringView waylandInterfacesKey{"X-QindaQt-KWin-Wayland-Interfaces"};
inline constexpr QLatin1StringView dbusRestrictedInterfacesKey{"X-QindaQt-KWin-DBus-Restricted-Interfaces"};

} // namespace QindaQt::CompositorNames
