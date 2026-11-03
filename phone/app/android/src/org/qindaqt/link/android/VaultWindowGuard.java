// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.android;

import android.app.Activity;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.WindowManager;
import java.lang.ref.WeakReference;

/**
 * FLAG_SECURE policy for the Qt activity window: secure while the vault page is shown OR while a
 * vault session holds decrypted data, so screenshots, screen recording and the recents thumbnail
 * stay blank for the whole unlocked session even if the user navigates elsewhere in the app.
 *
 * AGENT-GUARD: the flag is cleared only a moment after the last reason ends (so the final frame of an
 * unlocked screen cannot leak into a snapshot), and {@link #reapply} must be called when the app comes
 * back to the front because the activity may have been recreated. Qt owns the activity; this class
 * keeps only a weak reference to it.
 */
final class VaultWindowGuard {
    private static final long CLEAR_DELAY_MS = 1500L;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static WeakReference<Activity> last = new WeakReference<>(null);
    private static boolean page, session;
    private static final Runnable CLEAR = VaultWindowGuard::clearIfUnwanted;

    private VaultWindowGuard() {}

    static synchronized void setPage(Context ctx, boolean on) { remember(ctx); page = on; apply(); }
    static synchronized void setSession(boolean on) { session = on; apply(); }
    static synchronized void reapply(Context ctx) { remember(ctx); apply(); }

    private static void remember(Context ctx) {
        Activity a = VaultController.activityOf(ctx);
        if (a != null) last = new WeakReference<>(a);
    }

    private static void apply() {
        MAIN.removeCallbacks(CLEAR);
        if (page || session) setFlag(true);
        else MAIN.postDelayed(CLEAR, CLEAR_DELAY_MS);
    }

    private static synchronized void clearIfUnwanted() { if (!page && !session) setFlag(false); }

    private static void setFlag(boolean on) {
        final Activity a = last.get();
        if (a == null) return;
        a.runOnUiThread(() -> {
            if (on) a.getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
            else a.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_SECURE);
        });
    }
}
