// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/input_actions/input_actions.h"
namespace QindaQt::InputActions {
QStringList actionIds() {
    return {"none", "dictate", "voice-command", "toggle-gyro", "left-click", "right-click", "middle-click",
            "left-click-meta-shift", "right-click-meta-shift",
            "accept", "back", "up", "down", "left", "right", "overview",
            "launcher", "next-window", "previous-window", "maximize",
            "close-window", "desktop-left", "desktop-right", "show-desktop",
            "screenshot", "shortcut", "combination", "text", "hold-gyro", "hold-page", "macro", "pointer-up", "pointer-down", "pointer-left", "pointer-right", "next-page", "previous-page", "switch-page"};
}
}
