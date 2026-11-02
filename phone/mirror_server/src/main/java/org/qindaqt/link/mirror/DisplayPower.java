// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.os.Binder;
import android.os.IBinder;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.KeyCharacterMap;
import android.view.KeyEvent;

/**
 * Keeps the mirror display's power group awake while a session streams.
 *
 * <p>AGENT-NOTE: the display is created with OWN_DISPLAY_GROUP, so it has its own wakefulness.
 * When the phone is asleep, a new group starts non-interactive and Android drops input to it
 * (seen on SDK 37: "non-interactive displays={0, N}"). A screen wake lock bound to our display
 * id with ACQUIRE_CAUSES_WAKEUP wakes and holds only that group; the phone's own screen stays
 * as it was. Without that API the fallback injects KEYCODE_WAKEUP on our display, which some
 * releases route to the default display instead.
 *
 * <p>Lifetime: the wake lock token is a binder owned by this process; system_server drops the
 * lock if the server dies, so a crash never leaves the phone awake.
 */
final class DisplayPower {
    private static final int SCREEN_BRIGHT_WAKE_LOCK = 0x0000000a;
    private static final int ACQUIRE_CAUSES_WAKEUP = 0x10000000;

    private final HiddenApi api;
    private IBinder lock;

    DisplayPower(HiddenApi api) {
        this.api = api;
    }

    void keepAwake(int displayId) {
        if (api.acquireWakeLock != null) {
            IBinder token = new Binder();
            try {
                HiddenApi.call(api.acquireWakeLock, api.powerManager, token,
                        SCREEN_BRIGHT_WAKE_LOCK | ACQUIRE_CAUSES_WAKEUP, "qindalink-mirror",
                        HiddenApi.SHELL_PACKAGE, null, null, displayId, null);
                lock = token;
                Ln.i("display " + displayId + " held awake (" + interactivity(displayId) + ")");
                return;
            } catch (Exception e) {
                Ln.w("per-display wake lock refused: " + Ln.describe(e));
            }
        }
        wakeWithKey(displayId);
    }

    void release() {
        if (lock == null || api.releaseWakeLock == null) {
            return;
        }
        try {
            HiddenApi.call(api.releaseWakeLock, api.powerManager, lock, 0);
        } catch (Exception e) {
            Ln.w("wake lock release failed: " + Ln.describe(e));
        }
        lock = null;
    }

    String interactivity(int displayId) {
        if (api.isDisplayInteractive == null) {
            return "interactivity unknown";
        }
        try {
            Object value = HiddenApi.call(api.isDisplayInteractive, api.powerManager, displayId);
            return Boolean.TRUE.equals(value) ? "interactive" : "not interactive";
        } catch (Exception e) {
            return "interactivity unknown";
        }
    }

    private void wakeWithKey(int displayId) {
        long now = SystemClock.uptimeMillis();
        for (int action : new int[] {KeyEvent.ACTION_DOWN, KeyEvent.ACTION_UP}) {
            KeyEvent event = new KeyEvent(now, now, action, KeyEvent.KEYCODE_WAKEUP, 0, 0,
                    KeyCharacterMap.VIRTUAL_KEYBOARD, 0, 0, InputDevice.SOURCE_KEYBOARD);
            try {
                HiddenApi.call(api.setDisplayId, event, displayId);
                HiddenApi.call(api.injectInputEvent, api.inputManager, event, 0);
            } catch (Exception e) {
                Ln.w("wake key failed: " + Ln.describe(e));
                return;
            }
        }
        Ln.i("display " + displayId + " woken with KEYCODE_WAKEUP (" + interactivity(displayId) + ")");
    }
}
