// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.os.Binder;
import android.os.IBinder;

/**
 * Keeps the mirror display's power group awake while a session streams.
 *
 * <p>AGENT-NOTE: the display asks for OWN_DISPLAY_GROUP. Where Android honours it, the display
 * has its own wakefulness, and a screen wake lock bound to its id with ACQUIRE_CAUSES_WAKEUP
 * wakes and holds only that group. On SDK 37 (SM-S948U, One UI) the flag is accepted but the
 * display still lands in display group 0, and the lock is recorded as powerGroupId=0: it would
 * hold the phone's own screen on and could wake it. So the lock is taken only when
 * DisplayInfo.displayGroupId is not 0; in group 0 the mirror follows the phone's own power
 * state and needs the phone awake.
 *
 * <p>AGENT-GUARD: never acquire a screen lock or inject a wake key that lands in the default
 * display group, and keep no KEYCODE_WAKEUP or global wakeUp fallback; those act on the phone's
 * main display. A display that cannot be awake without that fails with a clear message.
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

    /** Returns null when the display is (or is held) awake, or the reason it cannot be. */
    String keepAwake(int displayId) {
        int group = displayGroup(displayId);
        if (group == 0) {
            // A new display can report non-interactive for a moment after creation; in group 0
            // its state is the group's, so the phone's own display 0 decides.
            if ("interactive".equals(interactivity(displayId)) || "interactive".equals(interactivity(0))) {
                Ln.i("display " + displayId + " shares the phone's power group; no wake lock, it sleeps with the phone");
                return null;
            }
            return "the phone is asleep and this Android build keeps the mirror display in the "
                    + "phone's own power group; wake the phone and try again";
        }
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

    /** The display's DisplayInfo.displayGroupId; 0 (the phone's own group) when unknown. */
    private int displayGroup(int displayId) {
        if (api.getDisplayInfo == null) {
            return 0;
        }
        try {
            Object info = HiddenApi.call(api.getDisplayInfo, api.displayManager, displayId);
            return info == null ? 0 : info.getClass().getField("displayGroupId").getInt(info);
        } catch (Exception e) {
            Ln.w("display group unknown: " + Ln.describe(e));
            return 0;
        }
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
