// SPDX-License-Identifier: GPL-3.0-or-later
#include "settings_route_search_metadata.h"

#include <QCoreApplication>

namespace QindaQt::Apps::SettingsCenter {
namespace {

constexpr const char *SearchContext = "SettingsSearch";

// One translatable, comma-separated source string per list, so a translator
// sees the whole set of synonyms at once and may add or drop terms for the
// language instead of translating words one by one.
QStringList keywordList(const char *commaSeparated) {
  const QString translated =
      QCoreApplication::translate(SearchContext, commaSeparated);
  QStringList keywords;
  for (const QString &part : translated.split(QLatin1Char(','))) {
    const QString keyword = part.trimmed();
    if (!keyword.isEmpty()) {
      keywords.append(keyword);
    }
  }
  return keywords;
}

// AGENT-CONTRACT: ids and titles mirror InputPage.qml's `destinations`
// exactly; the page ignores an id it does not know, so a drift here would
// list a search result that silently opens the default sub-page.
// tst_settings_route_search compares these ids with the page's list.
QList<SettingsRouteDestination> inputDestinations() {
  return {
      {.id = QStringLiteral("pointers"),
       .title = QCoreApplication::translate(SearchContext, "Mouse & touchpad"),
       .keywords = keywordList(QT_TRANSLATE_NOOP(
           "SettingsSearch", "mouse, touchpad, trackpad, pointer, cursor "
                             "speed, scrolling, natural scrolling, tap to "
                             "click"))},
      {.id = QStringLiteral("tablet"),
       .title = QCoreApplication::translate(SearchContext, "Pen & tablet"),
       .keywords = keywordList(QT_TRANSLATE_NOOP(
           "SettingsSearch", "pen, stylus, drawing tablet, graphics tablet, "
                             "wacom, pen display, pressure, mapping"))},
      {.id = QStringLiteral("keyboard"),
       .title = QCoreApplication::translate(SearchContext, "Keyboard"),
       .keywords = keywordList(QT_TRANSLATE_NOOP(
           "SettingsSearch", "key repeat, repeat rate, numlock, keyboard "
                             "layout, typing"))},
      {.id = QStringLiteral("shortcuts"),
       .title = QCoreApplication::translate(SearchContext, "Shortcuts"),
       .keywords = keywordList(QT_TRANSLATE_NOOP(
           "SettingsSearch", "keyboard shortcuts, hotkeys, key bindings, "
                             "global shortcuts, custom commands"))},
      {.id = QStringLiteral("touch"),
       .title = QCoreApplication::translate(SearchContext, "Touch"),
       .keywords = keywordList(QT_TRANSLATE_NOOP(
           "SettingsSearch", "touchscreen, touch screen, swipe, gestures, "
                             "edge swipe, hold time"))},
  };
}

// AGENT-NOTE: keep each term specific to its route. The palette lists every
// match, so a term shared by two routes (for example "shortcut" on Voice as
// well as Input) pushes the intended page below an unrelated one.
const char *routeKeywords(SettingsRouteComponent component) {
  switch (component) {
  case SettingsRouteComponent::Notifications:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "alerts, do not disturb, dnd, quiet hours, "
                             "banners, notification sounds, mute apps");
  case SettingsRouteComponent::Appearance:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "theme, dark mode, light mode, color scheme, "
                             "fonts, wallpaper, background, window "
                             "decorations");
  case SettingsRouteComponent::Display:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "monitor, screen, resolution, refresh rate, "
                             "scale, scaling, orientation, rotate, "
                             "arrangement");
  case SettingsRouteComponent::Network:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "wifi, wi-fi, wireless, wlan, ethernet, wired, "
                             "vpn, internet, hotspot, airplane mode");
  case SettingsRouteComponent::Customize:
    // ADR-0267: the page switches and saves layout presets and holds the
    // panel auto-hide delay; applets and panels are edited on the panels
    // themselves, which the page explains, so "applets" still lands here.
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "layout presets, saved layouts, layout profile, "
                             "panel layout, taskbar, dock, menu bar, panel "
                             "auto-hide, hide delay, edit panels, applets");
  case SettingsRouteComponent::Audio:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "sound, volume, speakers, headphones, "
                             "microphone, mixer, output device, input device");
  case SettingsRouteComponent::Bluetooth:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "pairing, pair device, headset, wireless "
                             "devices, adapter");
  case SettingsRouteComponent::Power:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "battery, charging, power profile, performance, "
                             "power saver, brightness, keyboard backlight, "
                             "suspend, screen lock");
  case SettingsRouteComponent::Clipboard:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "copy, paste, clipboard history, clear history");
  case SettingsRouteComponent::Color:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "icc, color profile, colour profile, "
                             "calibration, color management");
  case SettingsRouteComponent::Accessibility:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "a11y, high contrast, reduce motion, reduced "
                             "motion, transparency, text size, large text");
  case SettingsRouteComponent::Input:
    // The five sub-pages carry the specific terms (mouse, keyboard, pen...).
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "peripherals, input devices, hid");
  case SettingsRouteComponent::Streaming:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "obs, recording, screen recording, stream, "
                             "broadcast, virtual camera, webcam");
  case SettingsRouteComponent::DateTime:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "clock, time zone, timezone, ntp, automatic "
                             "time, first day of week, region");
  case SettingsRouteComponent::Windows:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "focus, snapping, tiling, window groups, "
                             "workspaces, virtual desktops, docking");
  case SettingsRouteComponent::DefaultApplications:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "browser, email, mail, file manager, text "
                             "editor, media player, open with, file types");
  case SettingsRouteComponent::AboutComputer:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "system information, hostname, computer name, "
                             "hardware, cpu, processor, memory, ram, disk, "
                             "storage, version");
  case SettingsRouteComponent::Startup:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "autostart, login items, launch at login, "
                             "startup programs");
  case SettingsRouteComponent::Screensaver:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "idle, idle screen, blank screen, lock "
                             "timeout, screen saver");
  case SettingsRouteComponent::LoginScreen:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "sddm, greeter, automatic login, autologin, "
                             "default session");
  case SettingsRouteComponent::Voice:
    return QT_TRANSLATE_NOOP("SettingsSearch",
                             "dictation, speech, speech to text, "
                             "transcription, voice typing, push to talk");
  }
  return "";
}

} // namespace

void applyBuiltInSearchMetadata(SettingsRoute &route) {
  route.keywords = keywordList(routeKeywords(route.component));
  // AGENT-NOTE: only Input accepts a destination today. A route gains an
  // entry here only when its page reads requestedDestination and honours it,
  // including while it is already open (ADR-0257).
  route.destinations = route.component == SettingsRouteComponent::Input
                           ? inputDestinations()
                           : QList<SettingsRouteDestination>{};
}

} // namespace QindaQt::Apps::SettingsCenter
