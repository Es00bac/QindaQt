// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.android;

import android.content.Context;
import android.content.Intent;
import android.util.Log;
import java.util.Arrays;
import java.util.List;
import org.qindaqt.link.vault.PassphraseStrength;
import org.qindaqt.link.vault.VaultBackup;
import org.qindaqt.link.vault.VaultException;
import org.qindaqt.link.vault.VaultRecord;
import org.qindaqt.link.vault.VaultLifecycle;
import org.qindaqt.link.vault.VaultSession;

/**
 * Encrypted backup export and import, driven through {@link VaultBackupActivity} (Storage Access
 * Framework). Secrets here are the backup passphrase (char[], zeroed after use) and the already
 * encrypted export bytes; plaintext entries never leave the vault package except as wiped records.
 *
 * AGENT-GUARD: there is no plaintext export path. Export encrypts first (PBKDF2 on a worker thread)
 * and only then opens the file picker; import decrypts on a worker thread and merges into an
 * UNLOCKED session or stages a restore for a new vault.
 */
final class VaultBackupFlow {
    private static final String TAG = "QindaLinkVault";
    private final VaultController c;
    private String op = "", status = "none";
    private int count;
    private byte[] pendingExport;
    private char[] importPass;
    private String importMode = "merge";

    VaultBackupFlow(VaultController c) { this.c = c; }

    synchronized String op() { return op; }
    synchronized String status() { return status; }
    synchronized int count() { return count; }

    synchronized void clear() { op = ""; status = "none"; count = 0; c.bump(); }

    private void set(String o, String s, int n) { op = o; status = s; count = n; c.bump(); }

    /** Returns "" when the export started, else a user-facing reason. */
    String startExport(Context ctx, char[] p1, char[] p2) {
        try {
            if (c.state() != VaultLifecycle.State.UNLOCKED) return "Unlock the vault first.";
            if (!Arrays.equals(p1, p2)) return "The passphrases do not match.";
            if (!PassphraseStrength.check(p1).ok) return "Use at least 12 characters or 4 words.";
            synchronized (this) { set("export", "working", 0); }
            final char[] pass = p1.clone();
            new Thread(() -> encryptThenPick(ctx, pass), "vault-export").start();
            return "";
        } finally { Arrays.fill(p1, ' '); Arrays.fill(p2, ' '); }
    }

    private void encryptThenPick(Context ctx, char[] pass) {
        List<VaultRecord> recs = null;
        try {
            VaultSession s = c.session();
            if (s == null) throw new VaultException(VaultException.Reason.LOCKED, "locked");
            recs = s.exportRecords();
            byte[] bytes = VaultBackup.export(recs, pass, VaultBackup.calibrate(600));
            synchronized (this) { pendingExport = bytes; set("export", "picking", recs.size()); }
            ctx.startActivity(new Intent(ctx, VaultBackupActivity.class).putExtra("op", "export").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } catch (VaultException | RuntimeException e) {
            Log.w(TAG, "export failed");
            synchronized (this) { set("export", "failed", 0); }
        } finally {
            Arrays.fill(pass, ' ');
            if (recs != null) VaultController.wipeAll(recs);
        }
    }

    /** Called by the activity with the picked file's content has been written (or not). */
    byte[] takeExport() {
        synchronized (this) { byte[] b = pendingExport; pendingExport = null; return b; }
    }

    synchronized void exportFinished(boolean ok, boolean cancelled, int n) {
        set("export", ok ? "done" : cancelled ? "cancelled" : "failed", ok ? count : 0);
    }

    /**
     * Mode: "merge" or "replace" (vault unlocked), or "restore" / "restore-replace" (no usable vault).
     * A restore is refused unless the vault is absent or known unusable (never from LOCKED), and
     * replacing an existing unusable vault needs the explicit "restore-replace" the UI sends after the
     * "this replaces the existing vault" confirmation. Returns "" when the picker opens.
     */
    String startImport(Context ctx, char[] pass, String mode) {
        VaultLifecycle.State st = c.state();
        boolean restore = mode.equals("restore") || mode.equals("restore-replace");
        String refusal = "";
        if (!restore && !mode.equals("merge") && !mode.equals("replace")) refusal = "Unknown import mode.";
        else if (restore && !VaultLifecycle.canRestore(st)) refusal = "A restore is only possible when there is no vault or it cannot be opened.";
        else if (restore && VaultLifecycle.restoreNeedsReplaceConfirm(st) && !mode.equals("restore-replace"))
            refusal = "Confirm that this replaces the existing vault first.";
        else if (!restore && st != VaultLifecycle.State.UNLOCKED) refusal = "Unlock the vault first.";
        if (!refusal.isEmpty()) { Arrays.fill(pass, ' '); return refusal; }
        synchronized (this) {
            if (importPass != null) Arrays.fill(importPass, ' ');
            importPass = pass; importMode = restore ? "restore" : mode;
            set("import", "picking", 0);
        }
        ctx.startActivity(new Intent(ctx, VaultBackupActivity.class).putExtra("op", "import").addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        return "";
    }

    /** Decrypts and applies picked backup bytes. Runs on the activity's worker thread. */
    void applyImport(byte[] file) {
        char[] pass; String mode;
        synchronized (this) { pass = importPass; importPass = null; mode = importMode; set("import", "working", 0); }
        List<VaultRecord> recs = null;
        try {
            if (pass == null) throw new VaultException(VaultException.Reason.INVALID, "no passphrase");
            recs = VaultBackup.read(file, pass);
            int n = recs.size();
            if (mode.equals("restore")) {
                if (!c.stageRestore(recs)) throw new VaultException(VaultException.Reason.INVALID, "restore not allowed now");
                recs = null;   // ownership moved to the controller
            } else {
                n = c.importInto(recs, mode.equals("replace"));   // transactional: applied only after the write
            }
            synchronized (this) { set("import", "done", n); }
        } catch (VaultException e) {
            Log.w(TAG, "import failed: " + e.reason());
            synchronized (this) { set("import", e.reason() == VaultException.Reason.WRONG_PASSPHRASE ? "wrong-passphrase" : "failed", 0); }
        } catch (java.io.IOException | RuntimeException e) {
            synchronized (this) { set("import", "failed", 0); }
        } finally {
            if (pass != null) Arrays.fill(pass, ' ');
            if (recs != null) VaultController.wipeAll(recs);
        }
    }

    synchronized void importCancelled() {
        if (importPass != null) { Arrays.fill(importPass, ' '); importPass = null; }
        set("import", "cancelled", 0);
    }
}
