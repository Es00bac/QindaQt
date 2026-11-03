// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.hardware.display.VirtualDisplayConfig;
import android.view.Surface;

/**
 * Owns one virtual display created through IDisplayManager as the shell user.
 *
 * <p>Lifetime: the display exists from {@link #create} until {@link #release} or process death.
 * system_server links to the callback binder's death, so a crashed server still releases the
 * display and (through DESTROY_CONTENT_ON_REMOVAL) the app task placed on it.
 *
 * <p>AGENT-NOTE: flag values are android.hardware.display.DisplayManager hidden constants. The
 * ladder tries the trusted set first and drops privileged flags on SecurityException, so a
 * shell user without ADD_TRUSTED_DISPLAY still gets a working (untrusted) display.
 */
final class VirtualDisplayHost {
    static final int FLAG_PUBLIC = 1;
    static final int FLAG_OWN_CONTENT_ONLY = 1 << 3;
    static final int FLAG_SUPPORTS_TOUCH = 1 << 6;
    static final int FLAG_DESTROY_CONTENT_ON_REMOVAL = 1 << 8;
    static final int FLAG_TRUSTED = 1 << 10;
    static final int FLAG_OWN_DISPLAY_GROUP = 1 << 11;
    static final int FLAG_ALWAYS_UNLOCKED = 1 << 12;
    static final int FLAG_TOUCH_FEEDBACK_DISABLED = 1 << 13;
    static final int FLAG_OWN_FOCUS = 1 << 14;
    static final int FLAG_STEAL_TOP_FOCUS_DISABLED = 1 << 16;

    static final int BASE_FLAGS = FLAG_PUBLIC | FLAG_OWN_CONTENT_ONLY | FLAG_SUPPORTS_TOUCH
            | FLAG_DESTROY_CONTENT_ON_REMOVAL;
    static final int TRUSTED_FLAGS = FLAG_TRUSTED | FLAG_OWN_DISPLAY_GROUP | FLAG_ALWAYS_UNLOCKED
            | FLAG_TOUCH_FEEDBACK_DISABLED | FLAG_OWN_FOCUS | FLAG_STEAL_TOP_FOCUS_DISABLED;

    /** WindowManager DISPLAY_IME_POLICY_HIDE: desktop text input replaces the phone keyboard. */
    private static final int IME_POLICY_HIDE = 2;

    private final HiddenApi api;
    private Object callback;
    private int displayId = -1;
    private int flags;

    VirtualDisplayHost(HiddenApi api) {
        this.api = api;
    }

    int displayId() {
        return displayId;
    }

    int flags() {
        return flags;
    }

    int create(String name, int width, int height, int dpi, Surface surface) throws Exception {
        int[] ladder = {BASE_FLAGS | TRUSTED_FLAGS, BASE_FLAGS, BASE_FLAGS & ~FLAG_PUBLIC};
        Exception last = null;
        for (int candidate : ladder) {
            Object token = api.newDisplayCallback();
            VirtualDisplayConfig config = new VirtualDisplayConfig.Builder(name, width, height, dpi)
                    .setFlags(candidate)
                    .setSurface(surface)
                    .build();
            try {
                int id = (Integer) HiddenApi.call(api.createVirtualDisplay, api.displayManager, config,
                        token, null, HiddenApi.SHELL_PACKAGE);
                if (id < 0) {
                    last = new IllegalStateException("createVirtualDisplay returned " + id);
                    continue;
                }
                callback = token;
                displayId = id;
                flags = candidate;
                hideImeOnDisplay();
                return id;
            } catch (SecurityException e) {
                Ln.w("display flags 0x" + Integer.toHexString(candidate) + " refused: " + Ln.describe(e));
                last = e;
            }
        }
        throw new IllegalStateException("could not create a virtual display", last);
    }

    void resize(int width, int height, int dpi) throws Exception {
        requireDisplay();
        HiddenApi.call(api.resizeVirtualDisplay, api.displayManager, callback, width, height, dpi);
    }

    void setSurface(Surface surface) throws Exception {
        requireDisplay();
        HiddenApi.call(api.setVirtualDisplaySurface, api.displayManager, callback, surface);
    }

    /** Detaches the encoder surface during teardown; failures only matter for logging. */
    void setSurfaceQuietly() {
        if (callback == null) {
            return;
        }
        try {
            HiddenApi.call(api.setVirtualDisplaySurface, api.displayManager, callback, null);
        } catch (Exception e) {
            Ln.d("surface detach failed: " + Ln.describe(e));
        }
    }

    void release() {
        if (callback == null) {
            return;
        }
        try {
            HiddenApi.call(api.releaseVirtualDisplay, api.displayManager, callback);
        } catch (Exception e) {
            Ln.e("releaseVirtualDisplay failed", e);
        }
        callback = null;
        displayId = -1;
    }

    private void hideImeOnDisplay() {
        if (api.setDisplayImePolicy == null) {
            return;
        }
        try {
            HiddenApi.call(api.setDisplayImePolicy, api.windowManager, displayId, IME_POLICY_HIDE);
        } catch (Exception e) {
            Ln.w("could not hide the phone keyboard on display " + displayId + ": " + Ln.describe(e));
        }
    }

    private void requireDisplay() {
        if (callback == null) {
            throw new IllegalStateException("no virtual display");
        }
    }
}
