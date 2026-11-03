// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.android;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricPrompt;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.security.keystore.KeyPermanentlyInvalidatedException;
import android.util.AtomicFile;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.security.UnrecoverableKeyException;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import javax.crypto.Cipher;
import org.qindaqt.link.vault.KeyPresence;
import org.qindaqt.link.vault.VaultEntry;
import org.qindaqt.link.vault.VaultException;
import org.qindaqt.link.vault.VaultLifecycle;
import org.qindaqt.link.vault.VaultLifecycle.State;
import org.qindaqt.link.vault.VaultRecord;
import org.qindaqt.link.vault.VaultSession;

/**
 * Owns the phone vault: file, Keystore keys, authentication prompt, session and every lock trigger.
 * The pure format and the state rules live in org.qindaqt.link.vault (VaultLifecycle); nothing there
 * knows about Android.
 *
 * AGENT-GUARD: never log titles, usernames, URLs, notes or passwords, and never put them in an
 * exception message; this class logs only state names and counts.
 * AGENT-GUARD: a failure to USE the key (not authenticated, device locked, Keystore busy) is retryable
 * and returns to LOCKED; only an integrity failure reaches ERROR and only a permanently invalidated
 * key reaches INVALIDATED (persisted in prefs, because the stale key can still exist). Recovery
 * ("Restore from backup") replaces the vault, so offering it for a transient failure destroys data.
 * AGENT-GUARD: replacing the vault creates the new key in the other slot and deletes the old key and
 * file contents only after the new file was written.
 * Threading: public methods are synchronized and callable from the Qt GUI thread; BiometricPrompt
 * callbacks arrive on the main executor and take the same lock.
 * AGENT-CONTRACT: {@link VaultBridge} is the only caller and C++ VaultBridge.cpp calls it by name.
 */
final class VaultController {
    private static final String TAG = "QindaLinkVault";
    static final long BACKGROUND_LOCK_MS = 30_000L, TICK_MS = 10_000L;
    private static final String RETRY = "Authentication failed. Try again.";
    private static final String KEYSTORE_RETRY = "The key store is unavailable right now. Try again.";

    private static VaultController instance;

    static synchronized VaultController get(Context ctx) {
        if (instance == null) instance = new VaultController(ctx.getApplicationContext());
        return instance;
    }

    private final Context app;
    private final Handler main = new Handler(Looper.getMainLooper());
    private final File file;
    private final SharedPreferences prefs;
    private State state = State.NONE;
    private String message = "";
    private VaultSession session;
    private int rev = 1;
    private long lastTouch = SystemClock.elapsedRealtime();
    private List<VaultRecord> pendingRestore;
    private int pendingCreateSlot = -1;
    final VaultBackupFlow backup = new VaultBackupFlow(this);

    private VaultController(Context app) {
        this.app = app;
        this.file = new File(app.getFilesDir(), "vault.qlvt");
        this.prefs = app.getSharedPreferences("qindalink_vault", Context.MODE_PRIVATE);
        recompute();
        app.registerReceiver(new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) { lock(); }
        }, new IntentFilter(Intent.ACTION_SCREEN_OFF));
        main.postDelayed(this::tick, TICK_MS);
    }

    // ---- state ---------------------------------------------------------------------------

    private void to(State s) {
        state = s;
        VaultWindowGuard.setSession(s == State.UNLOCKED || s == State.RESTORE_READY);
    }

    private void recompute() {
        if (session != null && session.isUnlocked()) { to(State.UNLOCKED); return; }
        KeyguardManager km = (KeyguardManager) app.getSystemService(Context.KEYGUARD_SERVICE);
        if (!file.exists()) { to(km.isDeviceSecure() ? State.NONE : State.NOLOCK); return; }
        int slot = fileSlot(null);
        KeyPresence p = slot >= 0 ? VaultKeys.presence(slot) : KeyPresence.any(VaultKeys.presence(0), VaultKeys.presence(1));
        switch (VaultLifecycle.decideLocked(slot, p, prefs.getInt("invalidatedSlot", VaultLifecycle.NO_SLOT))) {
            case INVALIDATED: markInvalidated(slot); break;
            case KEY_STORE_RETRY: to(State.LOCKED); message = KEYSTORE_RETRY; break;   // transient: never persisted
            default: to(State.LOCKED);
        }
    }

    /** Persists "this slot's key is gone" (only for a definite loss) so the stale key cannot flip the state back. */
    private void markInvalidated(int slot) {
        prefs.edit().putInt("invalidatedSlot", slot).commit();
        to(State.INVALIDATED);
        message = "The vault key is gone (the screen lock was removed or reset). This vault can no longer be opened "
            + "on this phone. Restore it from a backup, or erase it and start over.";
    }

    /** Keystore slot named by the file header, or -1 when unreadable. {@code bytes} may be null to read the file. */
    private int fileSlot(byte[] bytes) {
        try {
            byte[] b = bytes != null ? bytes : readFile();
            return (VaultSession.flagsOf(b) & VaultSession.FLAG_KEY_B) != 0 ? 1 : 0;
        } catch (VaultException | IOException e) { return -1; }
    }

    synchronized State state() { return state; }
    synchronized String message() { return message; }
    synchronized int rev() { return rev; }
    synchronized boolean strongBox() { return session != null && session.strongBox(); }
    synchronized VaultSession session() { return session; }
    synchronized boolean copyEnabled() { return prefs.getBoolean("copy", false); }
    synchronized void setCopyEnabled(boolean on) { prefs.edit().putBoolean("copy", on).apply(); rev++; }
    synchronized boolean fileExists() { return file.exists(); }
    synchronized int pendingRestoreCount() { return pendingRestore == null ? 0 : pendingRestore.size(); }

    synchronized void touch() {
        lastTouch = SystemClock.elapsedRealtime();
        if (session != null) session.touch();
    }

    // ---- locking -------------------------------------------------------------------------

    /** Zeroes the DEK, any decrypted restore staging and our clipboard copy; returns to the locked state. */
    synchronized void lock() {
        main.removeCallbacks(backgroundLock);
        if (session != null) { session.lock(); session = null; rev++; }
        if (pendingRestore != null) { wipeAll(pendingRestore); pendingRestore = null; rev++; }
        VaultClipboard.clearNow(app);
        if (state == State.UNLOCKED || state == State.RESTORE_READY) { message = ""; recompute(); rev++; }
    }

    private final Runnable backgroundLock = this::lock;

    /** The UI went to the background (false) or came back (true); 30 s without UI locks. */
    synchronized void setForeground(boolean fg) {
        main.removeCallbacks(backgroundLock);
        if (fg) touch();
        else if (session != null || pendingRestore != null) main.postDelayed(backgroundLock, BACKGROUND_LOCK_MS);
    }

    private synchronized void tick() {
        if ((session != null || pendingRestore != null)
                && SystemClock.elapsedRealtime() - lastTouch >= VaultSession.IDLE_LOCK_MILLIS) lock();
        main.postDelayed(this::tick, TICK_MS);
    }

    // ---- unlock / create -----------------------------------------------------------------

    synchronized void requestUnlock(Context ctx) {
        if (state == State.UNLOCKED || state == State.AUTHENTICATING) return;
        if (VaultLifecycle.canCreate(state)) { createVault(ctx); return; }
        if (state == State.INVALIDATED || state == State.NOLOCK) return;   // recovery or settings only
        if (!file.exists()) { recompute(); rev++; return; }
        try {
            byte[] bytes = readFile();
            int slot = (VaultSession.flagsOf(bytes) & VaultSession.FLAG_KEY_B) != 0 ? 1 : 0;
            Cipher c = VaultKeys.decryptCipher(slot, VaultSession.wrappedKeyOf(bytes));
            to(State.AUTHENTICATING); message = "";
            authenticate(ctx, c, "Unlock vault", authed -> openWith(bytes, slot, authed));
        } catch (KeyPermanentlyInvalidatedException e) {
            markInvalidated(fileSlot(null));
        } catch (UnrecoverableKeyException e) {
            // "No key": gone for good only when the Keystore definitely has no entry for the file's slot;
            // a Keystore error is a retry, never a persisted loss.
            int slot = fileSlot(null);
            KeyPresence p = slot >= 0 ? VaultKeys.presence(slot) : KeyPresence.ERROR;
            if (p == KeyPresence.ABSENT) markInvalidated(slot);
            else { to(State.LOCKED); message = p == KeyPresence.ERROR ? KEYSTORE_RETRY : RETRY; }
        } catch (VaultException e) {
            failIntegrity(e.reason());
        } catch (GeneralSecurityException | IOException | RuntimeException e) {
            retryLater();
            Log.w(TAG, "unlock preparation failed: " + e.getClass().getSimpleName());
        }
        rev++;
    }

    private void retryLater() { to(State.LOCKED); message = RETRY; }

    private void failIntegrity(VaultException.Reason r) {
        to(VaultLifecycle.afterOpenFailure(r));
        message = r == VaultException.Reason.ROLLBACK
            ? "This vault file is older than one this phone already opened, so it was not opened (an old copy may have been put back). Restore from a backup if that is wanted."
            : r == VaultException.Reason.UNSUPPORTED_VERSION ? "This vault was made by a newer version of the app."
            : "The vault file failed its integrity check and was not opened. Restore it from a backup.";
    }

    private synchronized void openWith(byte[] bytes, int slot, Cipher authed) {
        try {
            session = VaultSession.open(bytes, new VaultKeys.Authorized(authed), System::currentTimeMillis, prefs.getLong("gen", 0));
            prefs.edit().putLong("gen", Math.max(prefs.getLong("gen", 0), session.generation())).commit();
            to(State.UNLOCKED); message = ""; touch();
        } catch (VaultException e) {
            session = null;
            State next = VaultLifecycle.afterOpenFailure(e.reason());
            if (next == State.INVALIDATED) markInvalidated(slot);
            else if (next == State.LOCKED) retryLater();
            else failIntegrity(e.reason());
            Log.w(TAG, "open failed: " + e.reason());
        }
        rev++;
    }

    /** Creates a vault in a fresh Keystore slot; fills it from {@link #pendingRestore} when set. */
    synchronized void createVault(Context ctx) {
        if (!VaultLifecycle.canCreate(state)) return;
        int existing = file.exists() ? fileSlot(null) : -1;
        // Anything but a definite ABSENT counts as "slot 0 may exist": never overwrite a key we cannot see.
        int slot = VaultLifecycle.nextKeySlot(existing, VaultKeys.presence(0) != KeyPresence.ABSENT);
        State before = state;
        try {
            boolean sb = VaultKeys.generate(slot);
            Cipher c = VaultKeys.encryptCipher(slot);
            pendingCreateSlot = slot;
            to(State.AUTHENTICATING); message = "";
            authenticate(ctx, c, "Create vault", authed -> finishCreate(authed, sb, slot));
        } catch (GeneralSecurityException | IOException | RuntimeException e) {
            VaultKeys.delete(slot);
            pendingCreateSlot = -1;
            to(before);
            message = "A secure lock screen is required to create the vault.";
            Log.w(TAG, "key generation failed: " + e.getClass().getSimpleName());
        }
        rev++;
    }

    private synchronized void finishCreate(Cipher authed, boolean strongBox, int slot) {
        pendingCreateSlot = -1;
        VaultSession s = null;
        boolean written = false;
        try {
            int flags = (strongBox ? VaultSession.FLAG_STRONGBOX : 0) | (slot == 1 ? VaultSession.FLAG_KEY_B : 0);
            s = VaultSession.create(new VaultKeys.Authorized(authed), flags, System::currentTimeMillis);
            if (pendingRestore != null) s.importRecords(pendingRestore, true);
            writeFile(s.serialize());
            written = true;
            // Order matters (crash safety): the file now names the new slot; record that in prefs FIRST, and
            // only then delete the old key. The invalidated marker is slot-bound, so even a crash right here
            // cannot make a stale marker strand the new vault.
            session = s;
            boolean saved = prefs.edit().putInt("invalidatedSlot", VaultLifecycle.NO_SLOT).putLong("gen", s.generation()).commit();
            if (saved) VaultKeys.delete(1 - slot);   // if prefs failed keep the old key; it is harmless
            if (pendingRestore != null) { wipeAll(pendingRestore); pendingRestore = null; }
            to(State.UNLOCKED); message = ""; touch();
        } catch (VaultException | IOException | RuntimeException e) {
            if (s != null) s.lock();                 // zero the new DEK
            session = null;
            if (!written) VaultKeys.delete(slot);    // never delete a key the file on disk already names
            if (pendingRestore != null) { to(State.RESTORE_READY); message = "The vault could not be created. Your existing vault was not changed."; }
            else { recompute(); message = "The vault could not be created."; }
            Log.w(TAG, "create failed: " + e.getClass().getSimpleName());
        }
        rev++;
    }

    synchronized void eraseVault() {
        if (!VaultLifecycle.canErase(state)) return;
        lock();
        file.delete();
        VaultKeys.delete(0);
        VaultKeys.delete(1);
        prefs.edit().putInt("invalidatedSlot", VaultLifecycle.NO_SLOT).putLong("gen", 0).commit();
        message = "";
        recompute();
        rev++;
    }

    // ---- storage -------------------------------------------------------------------------

    /** Persists the unlocked session; call after every mutation. */
    synchronized void save() throws VaultException, IOException {
        if (session == null) throw new VaultException(VaultException.Reason.LOCKED, "vault is locked");
        writeFile(session.serialize());
        noteGeneration(session.generation());
        rev++;
    }

    /**
     * Transactional import: the new state is built and written first and applied to the session only
     * after the write succeeded, so a failure leaves memory and disk exactly as they were.
     */
    synchronized int importInto(List<VaultRecord> records, boolean replace) throws VaultException, IOException {
        if (session == null) throw new VaultException(VaultException.Reason.LOCKED, "vault is locked");
        VaultSession.Staged st = session.stageImport(records, replace);
        writeFile(session.serializeStaged(st));
        session.commit(st);
        noteGeneration(session.generation());
        rev++;
        return st.count();
    }

    private void noteGeneration(long g) { prefs.edit().putLong("gen", Math.max(prefs.getLong("gen", 0), g)).commit(); }

    private byte[] readFile() throws IOException { return new AtomicFile(file).readFully(); }

    private void writeFile(byte[] bytes) throws IOException {
        AtomicFile af = new AtomicFile(file);
        FileOutputStream out = af.startWrite();
        try { out.write(bytes); af.finishWrite(out); } catch (IOException e) { af.failWrite(out); throw e; }
    }

    // ---- restore staging (used by VaultBackupFlow) ----------------------------------------

    /** Keeps decrypted backup records for {@link #createVault}; refused unless a restore is allowed now. */
    synchronized boolean stageRestore(List<VaultRecord> records) {
        if (!VaultLifecycle.canRestore(state)) return false;
        if (pendingRestore != null) wipeAll(pendingRestore);
        pendingRestore = new ArrayList<>(records);
        to(State.RESTORE_READY);
        message = "Backup decrypted. Unlock to create a new vault from it.";
        touch();
        rev++;
        return true;
    }

    synchronized void bump() { rev++; }

    static void wipeAll(List<VaultRecord> l) { for (VaultRecord r : l) r.wipe(); l.clear(); }

    // ---- listing -------------------------------------------------------------------------

    synchronized List<VaultEntry> list() throws VaultException {
        if (session == null) throw new VaultException(VaultException.Reason.LOCKED, "vault is locked");
        return session.list();
    }

    // ---- authentication ------------------------------------------------------------------

    private void authenticate(Context ctx, Cipher cipher, String title, Consumer<Cipher> ok) {
        BiometricPrompt prompt = new BiometricPrompt.Builder(ctx)
            .setTitle(title)
            .setSubtitle("QindaLink")
            .setAllowedAuthenticators(BiometricManager.Authenticators.BIOMETRIC_STRONG | BiometricManager.Authenticators.DEVICE_CREDENTIAL)
            .setConfirmationRequired(false)
            .build();
        prompt.authenticate(new BiometricPrompt.CryptoObject(cipher), new CancellationSignal(), ctx.getMainExecutor(),
            new BiometricPrompt.AuthenticationCallback() {
                @Override public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult r) {
                    Cipher c = r.getCryptoObject() == null ? null : r.getCryptoObject().getCipher();
                    if (c == null) { cancelled("Authentication did not provide a key."); return; }
                    ok.accept(c);
                }
                @Override public void onAuthenticationError(int code, CharSequence msg) { cancelled("Authentication cancelled."); }
            });
    }

    private synchronized void cancelled(String msg) {
        if (state == State.AUTHENTICATING) {
            if (pendingCreateSlot >= 0) { VaultKeys.delete(pendingCreateSlot); pendingCreateSlot = -1; }
            if (pendingRestore != null) to(State.RESTORE_READY); else recompute();
            message = msg;
        }
        rev++;
    }

    static Activity activityOf(Context c) {
        while (c instanceof android.content.ContextWrapper) {
            if (c instanceof Activity) return (Activity) c;
            c = ((android.content.ContextWrapper) c).getBaseContext();
        }
        return null;
    }

    Context appContext() { return app; }
}
