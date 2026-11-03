// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

/**
 * What desktop input the server is willing to inject.
 *
 * <p>AGENT-GUARD: keys are an allowlist, not a blocklist. A key event carries the mirror
 * display id, but global keys (POWER, SLEEP, WAKEUP, HOME, ASSIST, APP_SWITCH, volume, call,
 * camera, brightness, META) are handled by the phone's window policy before any display
 * routing and would act on the whole phone. Only typing, editing, navigation and BACK pass.
 *
 * <p>Pure Java (no android.*) so phone/mirror_server/tests can run it on the host JVM.
 */
final class InputPolicy {
    // android.view.MotionEvent BUTTON_PRIMARY | SECONDARY | TERTIARY | BACK | FORWARD
    static final int ALLOWED_BUTTONS = 0x1 | 0x2 | 0x4 | 0x8 | 0x10;

    private InputPolicy() {}

    /** Clamps a coordinate to the display's last addressable pixel; NaN becomes 0. */
    static float clamp(float value, int size) {
        if (size <= 0 || Float.isNaN(value)) {
            return 0f;
        }
        return Math.max(0f, Math.min(size - 1, value));
    }

    static int buttons(int requested) {
        return requested & ALLOWED_BUTTONS;
    }

    static boolean keyAllowed(int keycode) {
        return keycode == 4 // BACK
                || (keycode >= 7 && keycode <= 16) // 0-9
                || (keycode >= 19 && keycode <= 23) // DPAD up/down/left/right/center
                || (keycode >= 29 && keycode <= 56) // A-Z, comma, period
                || (keycode >= 61 && keycode <= 62) // TAB, SPACE
                || (keycode >= 66 && keycode <= 77) // ENTER, DEL, punctuation, AT
                || keycode == 81 // PLUS
                || keycode == 82 // MENU (app menu, not a global key)
                || (keycode >= 92 && keycode <= 93) // PAGE_UP, PAGE_DOWN
                || (keycode >= 111 && keycode <= 112) // ESCAPE, FORWARD_DEL
                || (keycode >= 122 && keycode <= 124) // MOVE_HOME, MOVE_END, INSERT
                || (keycode >= 131 && keycode <= 142) // F1-F12
                || keycode == 160; // NUMPAD_ENTER
    }
}
