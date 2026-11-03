// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.android;

import android.content.Context;
import java.util.Arrays;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.qindaqt.link.vault.AutotypeSequence;
import org.qindaqt.link.vault.PassphraseStrength;
import org.qindaqt.link.vault.PasswordGenerator;
import org.qindaqt.link.vault.VaultEntry;
import org.qindaqt.link.vault.VaultException;
import org.qindaqt.link.vault.VaultRecord;
import org.qindaqt.link.vault.VaultSession;

/**
 * The single class the Qt/JNI layer calls for the vault (phone/app/VaultBridge.cpp). Static,
 * non-blocking, JSON-in/JSON-out, and safe when the vault is locked.
 *
 * AGENT-CONTRACT: method names and signatures are looked up by name from C++ via QJniObject; change
 * both sides together. Secrets cross this boundary only as an explicit reveal ({@link #revealJson}),
 * a save and a copy; nothing here logs a field value. The Qt side keeps a revealed value only while
 * the detail view shows it.
 */
public final class VaultBridge {
    private VaultBridge() {}

    private static VaultController c(Context ctx) { return VaultController.get(ctx); }

    /** {state, message, rev, strongBox, copyEnabled, count, backup:{op,status,count}, restoreCount}. */
    public static String stateJson(Context ctx) {
        VaultController v = c(ctx);
        JSONObject o = new JSONObject();
        try {
            o.put("state", v.state().name().toLowerCase(java.util.Locale.ROOT)).put("message", v.message()).put("rev", v.rev())
                .put("strongBox", v.strongBox()).put("copyEnabled", v.copyEnabled()).put("restoreCount", v.pendingRestoreCount());
            VaultSession s = v.session();
            o.put("count", s != null && s.isUnlocked() ? s.size() : 0);
            o.put("backup", new JSONObject().put("op", v.backup.op()).put("status", v.backup.status()).put("count", v.backup.count()));
        } catch (JSONException | VaultException ignored) { /* keys are constants; size() only fails when locked */ }
        return o.toString();
    }

    /** The non-secret list fields; "[]" while locked. */
    public static String listJson(Context ctx) {
        JSONArray a = new JSONArray();
        try {
            for (VaultEntry e : c(ctx).list())
                a.put(new JSONObject().put("id", e.id).put("title", e.title).put("username", e.username).put("url", e.url)
                    .put("updated", e.updatedMillis));
        } catch (JSONException | VaultException ignored) { /* locked: empty list */ }
        return a.toString();
    }

    /** Everything except password and notes, for the detail and editor screens. */
    public static String entryJson(Context ctx, String id) {
        VaultController v = c(ctx);
        try {
            VaultSession s = v.session();
            if (s == null) return "{}";
            VaultRecord r = s.get(id);
            try {
                return new JSONObject().put("id", r.id).put("title", r.title).put("username", r.username).put("url", r.url)
                    .put("autotype", r.autotype).toString();
            } finally { r.wipe(); }
        } catch (JSONException | VaultException e) { return "{}"; }
    }

    /** The secrets of one entry, produced only on an explicit reveal or edit. */
    public static String revealJson(Context ctx, String id) {
        VaultController v = c(ctx);
        try {
            VaultSession s = v.session();
            if (s == null) return "{}";
            VaultRecord r = s.get(id);
            try {
                return new JSONObject().put("password", r.passwordString()).put("notes", r.notesString()).toString();
            } finally { r.wipe(); }
        } catch (JSONException | VaultException e) { return "{}"; }
    }

    /** Saves an entry from JSON {id?, title, username, url, password, notes, autotype}. "ok:<id>" or "err:<reason>". */
    public static String saveEntry(Context ctx, String json) {
        VaultController v = c(ctx);
        VaultRecord r = new VaultRecord();
        try {
            JSONObject o = new JSONObject(json);
            String id = o.optString("id", "");
            r.id = id.isEmpty() ? null : id;
            r.title = o.optString("title", "").trim(); r.username = o.optString("username", "").trim(); r.url = o.optString("url", "").trim();
            r.setPassword(o.optString("password", "")).setNotes(o.optString("notes", ""));
            String at = o.optString("autotype", "").trim();
            r.autotype = at.isEmpty() ? VaultRecord.DEFAULT_AUTOTYPE : at;
            try { AutotypeSequence.parse(r.autotype); }
            catch (AutotypeSequence.SyntaxException e) { return "err:The autotype sequence is invalid near character " + e.position() + "."; }
            VaultSession s = v.session();
            if (s == null) return "err:The vault is locked.";
            r.updatedMillis = 0;
            String saved = s.put(r);
            v.save();
            return "ok:" + saved;
        } catch (JSONException | VaultException | java.io.IOException e) {
            return "err:" + (e instanceof VaultException && ((VaultException) e).reason() == VaultException.Reason.INVALID
                ? "Give the entry a title, username or URL." : "The entry could not be saved.");
        } finally { r.wipe(); }
    }

    public static boolean deleteEntry(Context ctx, String id) {
        VaultController v = c(ctx);
        try {
            VaultSession s = v.session();
            if (s == null) return false;
            s.delete(id);
            v.save();
            return true;
        } catch (VaultException | java.io.IOException e) { return false; }
    }

    public static void requestUnlock(Context ctx) { c(ctx).requestUnlock(ctx); }
    public static void lock(Context ctx) { c(ctx).lock(); }
    public static void touch(Context ctx) { c(ctx).touch(); }
    public static void setForeground(Context ctx, boolean fg) { c(ctx).setForeground(fg); }
    public static void eraseVault(Context ctx) { c(ctx).eraseVault(); }
    public static void setCopyEnabled(Context ctx, boolean on) { c(ctx).setCopyEnabled(on); }
    public static void clearBackupStatus(Context ctx) { c(ctx).backup.clear(); }

    /** The vault page is shown (true) or left (false); FLAG_SECURE also stays on while a session is unlocked. */
    public static void setSecure(Context ctx, boolean on) { VaultWindowGuard.setPage(ctx, on); }

    /** Re-applies the window flag when the app returns to the front (the activity may have been recreated). */
    public static void reapplySecure(Context ctx) { VaultWindowGuard.reapply(ctx); }

    public static String generatePassword(int length, boolean lower, boolean upper, boolean digits, boolean symbols, boolean avoidAmbiguous) {
        try {
            char[] p = new PasswordGenerator().password(length, lower, upper, digits, symbols, avoidAmbiguous);
            String s = new String(p);
            Arrays.fill(p, ' ');
            return s;
        } catch (IllegalArgumentException e) { return ""; }
    }

    public static String generatePassphrase(int words, String separator, boolean capitalize, boolean number) {
        try {
            char[] p = new PasswordGenerator().passphrase(words, separator, capitalize, number);
            String s = new String(p);
            Arrays.fill(p, ' ');
            return s;
        } catch (IllegalArgumentException e) { return ""; }
    }

    /** {ok, label, bits, reason} for a candidate backup passphrase. */
    public static String passphraseHint(String pass) {
        char[] p = pass.toCharArray();
        PassphraseStrength.Result r = PassphraseStrength.check(p);
        Arrays.fill(p, ' ');
        try { return new JSONObject().put("ok", r.ok).put("label", r.label).put("bits", r.bits).put("reason", r.reason).toString(); }
        catch (JSONException e) { return "{}"; }
    }

    public static String exportBackup(Context ctx, String p1, String p2) {
        return c(ctx).backup.startExport(ctx, p1.toCharArray(), p2.toCharArray());
    }

    public static String importBackup(Context ctx, String pass, String mode) {
        return c(ctx).backup.startImport(ctx, pass.toCharArray(), mode);
    }

    /** Copies the password when the user enabled copying: sensitive flag set, cleared after 30 s and on lock. */
    public static boolean copyPassword(Context ctx, String id) {
        VaultController v = c(ctx);
        if (!v.copyEnabled()) return false;
        VaultRecord r = null;
        try {
            VaultSession s = v.session();
            if (s == null) return false;
            r = s.get(id);
            return VaultClipboard.copy(ctx, r.passwordString());
        } catch (VaultException e) { return false; }
        finally { if (r != null) r.wipe(); }
    }
}
