// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.os.Binder;
import android.os.IBinder;

/**
 * Keeps the mirror display's power group awake while a session streams.
 *
 * <p>AGENT-NOTE: the display is created with OWN_DISPLAY_GROUP, so it has its own wakefulness.
 * When the phone is asleep, a new group starts non-interactive and Android drops input to it
 * (seen on SDK 37: "non-interactive displays={0, N}"). A screen wake lock bound to our display
 * id with ACQUIRE_CAUSES_WAKEUP wakes and holds only that group; the phone's own screen stays
 * as it was.
 *
 * <p>AGENT-GUARD: there is deliberately no KEYCODE_WAKEUP or global wakeUp fallback; on some
 * releases those wake the phone's main display. Without the per-display lock the session runs
 * only if the display is already interactive, and otherwise fails with a clear message.
 *
 * <p>Lifetime: the wake lock token is a binder owned by this process; system_server drops the
 * lock if the server dies, so a crash never leaves the display awake.
 */
final class DisplayPower {
    private static final int SCREEN_BRIGHT_WAKE_LOCK = 0x0000000a;
    private static final int ACQUIRE_CAUSES_WAKEUP = 0x10000000;

    private final HiddenApi api;
    private IBinder lock;

    DisplayPower(HiddenApi api) {
        this.api = api;
    }

    /** Returns null when the display is held awake, or the reason it cannot be. */
    String keepAwake(int displayId) {
        if (api.acquireWakeLock != null) {
            IBinder token = new Binder();
            try {
                HiddenApi.call(api.acquireWakeLock, api.powerManager, token,
                        SCREEN_BRIGHT_WAKE_LOCK | ACQUIRE_CAUSES_WAKEUP, "qindalink-mirror",
                        HiddenApi.SHELL_PACKAGE, null, null, displayId, null);
                lock = token;
                Ln.i("display " + displayId + " held awake (" + interactivity(displayId) + ")");
                return null;
            } catch (Exception e) {
                Ln.w("per-display wake lock refused: " + Ln.describe(e));
            }
        }
        if ("interactive".equals(interactivity(displayId))) {
            Ln.w("display " + displayId + " is awake but cannot be held; it sleeps with the phone");
            return null;
        }
        return "this phone cannot keep the mirror display awake without waking its own screen "
                + "(no per-display wake lock); wake the phone and try again";
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

    private String interactivity(int displayId) {
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
}
