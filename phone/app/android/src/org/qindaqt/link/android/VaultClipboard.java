// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.android;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.PersistableBundle;

/**
 * The optional "Copy password" clipboard path. The clip is flagged sensitive and cleared 30 s later.
 *
 * AGENT-GUARD: on Android 10+ a background app cannot read the clipboard description, so the clear is
 * unconditional (clearPrimaryClip) and guarded only by our own copy generation: if a newer copy of
 * ours replaced this one the older timer does nothing. The vault lock also clears. Reading the clip
 * to compare would fail silently and leave the password on the clipboard (the bug this fixes).
 */
final class VaultClipboard {
    private static final long CLEAR_MS = 30_000L;
    private static final Handler MAIN = new Handler(Looper.getMainLooper());
    private static int generation;
    private static boolean pending;

    private VaultClipboard() {}

    static synchronized void copy(Context ctx, String text) {
        Context app = ctx.getApplicationContext();
        ClipboardManager cm = (ClipboardManager) app.getSystemService(Context.CLIPBOARD_SERVICE);
        ClipData clip = ClipData.newPlainText("qindalink-vault", text);
        PersistableBundle extras = new PersistableBundle();
        extras.putBoolean("android.content.extra.IS_SENSITIVE", true);   // ClipDescription.EXTRA_IS_SENSITIVE
        clip.getDescription().setExtras(extras);
        cm.setPrimaryClip(clip);
        pending = true;
        final int mine = ++generation;
        MAIN.postDelayed(() -> expire(app, mine), CLEAR_MS);
    }

    private static synchronized void expire(Context app, int mine) {
        if (pending && mine == generation) clearNow(app);
    }

    /** Clears the clipboard if we put something on it and have not cleared it yet. */
    static synchronized void clearNow(Context ctx) {
        if (!pending) return;
        pending = false;
        generation++;
        try {
            ((ClipboardManager) ctx.getApplicationContext().getSystemService(Context.CLIPBOARD_SERVICE)).clearPrimaryClip();
        } catch (RuntimeException ignored) { /* nothing more we can do */ }
    }
}
