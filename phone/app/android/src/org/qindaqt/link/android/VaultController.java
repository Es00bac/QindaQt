// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.android;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.hardware.biometrics.BiometricPrompt;
import android.os.CancellationSignal;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.security.keystore.KeyPermanentlyInvalidatedException;
import android.util.AtomicFile;
import android.util.Log;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import javax.crypto.Cipher;
import org.qindaqt.link.vault.VaultEntry;
import org.qindaqt.link.vault.VaultException;
import org.qindaqt.link.vault.VaultRecord;
import org.qindaqt.link.vault.VaultSession;

/**
 * Owns the phone vault: file, Keystore key, authentication prompt, session and every lock trigger.
 * The pure format lives in org.qindaqt.link.vault; nothing there knows about Android.
 *
 * AGENT-GUARD: never log titles, usernames, URLs, notes or passwords, and never put them in an
 * exception message; this class logs only state names and counts. The vault session exists only in
 * this process; locking (idle 5 min, screen off, UI gone 30 s, explicit) zeroes the DEK.
 * Threading: public methods are synchronized and callable from the Qt GUI thread; BiometricPrompt
 * callbacks arrive on the main executor and take the same lock.
 * AGENT-CONTRACT: {@link VaultBridge} is the only caller and C++ VaultBridge.cpp calls it by name.
 */
final class VaultController {
    private static final String TAG = "QindaLinkVault";
    static final long BACKGROUND_LOCK_MS = 30_000L, TICK_MS = 10_000L;
    /** Lifecycle shown to the UI; the strings are part of the QML contract. */
    enum State { NONE, LOCKED, UNLOCKED, AUTHENTICATING, INVALIDATED, NOLOCK, ERROR, RESTORE_READY }

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

    private void recompute() {
        if (session != null && session.isUnlocked()) { state = State.UNLOCKED; return; }
        KeyguardManager km = (KeyguardManager) app.getSystemService(Context.KEYGUARD_SERVICE);
        if (!file.exists()) { state = km.isDeviceSecure() ? State.NONE : State.NOLOCK; return; }
        state = VaultKeys.exists() ? State.LOCKED : State.INVALIDATED;
        if (state == State.INVALIDATED) message = "The vault key was invalidated (a fingerprint was added or the screen lock was removed). "
            + "This vault can no longer be opened on this phone. Restore it from a backup.";
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

    synchronized void lock() {
        main.removeCallbacks(backgroundLock);
        if (session != null) { session.lock(); session = null; rev++; }
        if (state == State.UNLOCKED) { message = ""; recompute(); }
    }

    private final Runnable backgroundLock = this::lock;

    /** The UI went to the background (false) or came back (true); 30 s without UI locks. */
    synchronized void setForeground(boolean fg) {
        main.removeCallbacks(backgroundLock);
        if (fg) touch();
        else if (session != null) main.postDelayed(backgroundLock, BACKGROUND_LOCK_MS);
    }

    private synchronized void tick() {
        if (session != null && SystemClock.elapsedRealtime() - lastTouch >= VaultSession.IDLE_LOCK_MILLIS) lock();
        main.postDelayed(this::tick, TICK_MS);
    }

    // ---- unlock / create -----------------------------------------------------------------

    synchronized void requestUnlock(Context ctx) {
        if (state == State.UNLOCKED || state == State.AUTHENTICATING) return;
        if (state == State.NONE || state == State.RESTORE_READY) { createVault(ctx); return; }
        if (!file.exists()) { recompute(); return; }
        try {
            byte[] bytes = readFile();
            Cipher c = VaultKeys.decryptCipher(VaultSession.wrappedKeyOf(bytes));
            state = State.AUTHENTICATING; message = "";
            authenticate(ctx, c, "Unlock vault", authed -> openWith(bytes, authed));
        } catch (KeyPermanentlyInvalidatedException | java.security.UnrecoverableKeyException e) {
            state = State.INVALIDATED;
            recompute();
        } catch (GeneralSecurityException | IOException | VaultException e) {
            fail("The vault could not be opened. Restore it from a backup if this keeps happening.");
            Log.w(TAG, "unlock preparation failed: " + e.getClass().getSimpleName());
        }
        rev++;
    }

    private synchronized void openWith(byte[] bytes, Cipher authed) {
        try {
            session = VaultSession.open(bytes, new VaultKeys.Authorized(authed), System::currentTimeMillis);
            state = State.UNLOCKED; message = ""; touch();
        } catch (VaultException e) {
            session = null;
            fail(e.reason() == VaultException.Reason.TAMPERED || e.reason() == VaultException.Reason.CORRUPT
                ? "The vault file failed its integrity check and was not opened. Restore it from a backup."
                : "The vault could not be opened.");
            Log.w(TAG, "open failed: " + e.reason());
        }
        rev++;
    }

    /** Creates a vault with a new Keystore key; fills it from {@link #pendingRestore} when set. */
    synchronized void createVault(Context ctx) {
        if (state != State.NONE && state != State.RESTORE_READY) return;
        try {
            boolean sb = VaultKeys.generate();
            Cipher c = VaultKeys.encryptCipher();
            state = State.AUTHENTICATING; message = "";
            authenticate(ctx, c, "Create vault", authed -> finishCreate(authed, sb));
        } catch (GeneralSecurityException | IOException e) {
            fail("A secure lock screen is required to create the vault.");
            Log.w(TAG, "key generation failed: " + e.getClass().getSimpleName());
        }
        rev++;
    }

    private synchronized void finishCreate(Cipher authed, boolean strongBox) {
        try {
            session = VaultSession.create(new VaultKeys.Authorized(authed), strongBox, System::currentTimeMillis);
            if (pendingRestore != null) {
                session.importRecords(pendingRestore, true);
                wipeAll(pendingRestore);
                pendingRestore = null;
            }
            writeFile(session.serialize());
            state = State.UNLOCKED; message = ""; touch();
        } catch (VaultException | IOException e) {
            if (session != null) { session.lock(); session = null; }
            fail("The vault could not be created.");
            Log.w(TAG, "create failed: " + e.getClass().getSimpleName());
        }
        rev++;
    }

    synchronized void eraseVault() {
        if (state != State.INVALIDATED && state != State.ERROR && state != State.UNLOCKED) return;
        lock();
        file.delete();
        VaultKeys.delete();
        if (pendingRestore != null) { wipeAll(pendingRestore); pendingRestore = null; }
        message = "";
        recompute();
        rev++;
    }

    // ---- storage -------------------------------------------------------------------------

    /** Persists the unlocked session; call after every mutation. */
    synchronized void save() throws VaultException, IOException {
        if (session == null) throw new VaultException(VaultException.Reason.LOCKED, "vault is locked");
        writeFile(session.serialize());
        rev++;
    }

    private byte[] readFile() throws IOException {
        try (FileInputStream in = new FileInputStream(file)) {
            byte[] b = new byte[(int) file.length()];
            int n = 0;
            while (n < b.length) { int r = in.read(b, n, b.length - n); if (r < 0) throw new IOException("short read"); n += r; }
            return b;
        }
    }

    private void writeFile(byte[] bytes) throws IOException {
        AtomicFile af = new AtomicFile(file);
        FileOutputStream out = af.startWrite();
        try { out.write(bytes); af.finishWrite(out); } catch (IOException e) { af.failWrite(out); throw e; }
    }

    // ---- restore staging (used by VaultBackupFlow) ----------------------------------------

    synchronized void stageRestore(List<VaultRecord> records) {
        if (pendingRestore != null) wipeAll(pendingRestore);
        pendingRestore = new ArrayList<>(records);
        state = State.RESTORE_READY;
        message = "Backup decrypted. Unlock to create a new vault from it.";
        rev++;
    }

    synchronized void bump() { rev++; }

    static void wipeAll(List<VaultRecord> l) { for (VaultRecord r : l) r.wipe(); l.clear(); }

    private void fail(String msg) { state = State.ERROR; message = msg; }

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
            .setAllowedAuthenticators(android.hardware.biometrics.BiometricManager.Authenticators.BIOMETRIC_STRONG
                | android.hardware.biometrics.BiometricManager.Authenticators.DEVICE_CREDENTIAL)
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
            if (pendingRestore != null) state = State.RESTORE_READY; else recompute();
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
