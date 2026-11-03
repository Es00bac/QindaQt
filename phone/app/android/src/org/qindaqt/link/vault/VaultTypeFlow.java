// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.util.function.LongSupplier;
import java.util.function.Supplier;

/**
 * The phone-side state machine of "Type on desktop" (ADR-0440): ask the desktop which window has focus,
 * show it, wait for the user's confirmation (which lapses after 30 s), and only then build the steps from
 * the unlocked vault and send them.
 *
 * Threading: all methods are thread-safe (UI thread calls, session thread callbacks). Ownership: the
 * flow never holds a secret; the {@link VaultTypingSource} it creates is handed to the link, which closes it.
 * AGENT-GUARD: a secret is never built before {@link #confirm} succeeds, never sent without a focus token
 * the user saw, and never built while the vault is locked ({@link Phase#NEED_UNLOCK}: the UI unlocks and
 * calls confirm again, still within the original 30 s). Messages shown to the user never contain step text.
 */
public final class VaultTypeFlow {
    public static final long CONFIRM_LAPSE_MS = 30_000, FOCUS_TIMEOUT_MS = 10_000, SEND_TIMEOUT_MS = 90_000;

    public enum Phase { IDLE, REQUESTING, CONFIRM, NEED_UNLOCK, SENDING, DONE, FAILED }

    public interface FocusCallback {
        void onFocus(String token, String app, String title);
        void onError(String code);
    }
    public interface ResultCallback {
        void onDone();
        void onError(String code);
    }
    /** The desktop connection; implemented by {@link VaultTypeFeature}, faked in tests. */
    public interface DesktopLink {
        /** Asks {@code desktop.focus}; exactly one callback call, possibly on another thread. */
        void requestFocus(FocusCallback cb);
        /** Sends {@code vault.type}; the link closes {@code src} when done with it, whatever happens. */
        void sendType(String token, VaultTypingSource src, ResultCallback cb);
    }

    private final DesktopLink link;
    private final Supplier<VaultSession> session;
    private final LongSupplier clock;
    private Phase phase = Phase.IDLE;
    private String entryId, token, app = "", title = "", message = "";
    private VaultTypingSource.Field field;
    private long deadline, started, rev;
    private int generation;   // ignores callbacks of an attempt that was cancelled or replaced

    public VaultTypeFlow(DesktopLink link, Supplier<VaultSession> session, LongSupplier clock) {
        this.link = link;
        this.session = session;
        this.clock = clock;
    }

    /** Starts (or restarts) an attempt for one entry and field; asks the desktop for its focused window. */
    public void begin(String entryId, VaultTypingSource.Field field) {
        final int gen;
        synchronized (this) {
            gen = ++generation;
            this.entryId = entryId;
            this.field = field;
            token = null;
            app = title = "";
            message = "";
            started = clock.getAsLong();
            set(Phase.REQUESTING);
        }
        link.requestFocus(new FocusCallback() {
            @Override public void onFocus(String t, String a, String ti) {
                synchronized (VaultTypeFlow.this) {
                    if (gen != generation || phase != Phase.REQUESTING) return;
                    token = t;
                    app = a == null ? "" : a;
                    title = ti == null ? "" : ti;
                    deadline = clock.getAsLong() + CONFIRM_LAPSE_MS;
                    set(Phase.CONFIRM);
                }
            }
            @Override public void onError(String code) { fail(gen, Phase.REQUESTING, code); }
        });
    }

    /** The user confirmed the shown target. May ask for an unlock first. */
    public void confirm() {
        final int gen;
        final String tok, id;
        final VaultTypingSource.Field f;
        final VaultTypingSource src;
        synchronized (this) {
            if (phase != Phase.CONFIRM && phase != Phase.NEED_UNLOCK) return;
            if (clock.getAsLong() > deadline) {
                failLocked("The confirmation lapsed. Try again.");
                return;
            }
            VaultSession s = session.get();
            if (s == null || !s.isUnlocked()) {
                set(Phase.NEED_UNLOCK);
                return;
            }
            gen = generation;
            tok = token;
            id = entryId;
            f = field;
            try {
                src = new VaultTypingSource(s, id, f);
                try {
                    VaultWireSteps.validate(src.steps());
                } catch (VaultWireSteps.Unsupported | VaultException e) {
                    src.close();
                    failLocked(e instanceof VaultException ? lockedOrNot((VaultException) e) : e.getMessage());
                    return;
                }
            } catch (VaultException e) {
                failLocked(lockedOrNot(e));
                return;
            }
            token = null;           // single use
            started = clock.getAsLong();
            set(Phase.SENDING);
        }
        link.sendType(tok, src, new ResultCallback() {
            @Override public void onDone() {
                synchronized (VaultTypeFlow.this) {
                    if (gen != generation || phase != Phase.SENDING) return;
                    message = "Typed on the desktop.";
                    set(Phase.DONE);
                }
            }
            @Override public void onError(String code) { fail(gen, Phase.SENDING, code); }
        });
    }

    /** The user dismissed the sheet. Nothing is sent after this. */
    public synchronized void cancel() {
        generation++;
        token = null;
        message = "";
        set(Phase.IDLE);
    }

    /** Applies the time limits; the UI calls it with every poll. */
    public synchronized void poll() {
        long now = clock.getAsLong();
        if ((phase == Phase.CONFIRM || phase == Phase.NEED_UNLOCK) && now > deadline) failLocked("The confirmation lapsed. Try again.");
        else if (phase == Phase.REQUESTING && now - started > FOCUS_TIMEOUT_MS) failLocked("The desktop did not answer.");
        else if (phase == Phase.SENDING && now - started > SEND_TIMEOUT_MS) failLocked("The desktop did not finish typing.");
    }

    public synchronized Phase phase() { return phase; }
    public synchronized String app() { return app; }
    public synchronized String title() { return title; }
    public synchronized String message() { return message; }
    public synchronized long rev() { return rev; }

    private void fail(int gen, Phase expected, String code) {
        synchronized (this) {
            if (gen != generation || phase != expected) return;
            failLocked(messageFor(code));
        }
    }

    private void failLocked(String msg) {
        generation++;           // late callbacks of this attempt are ignored
        token = null;
        message = msg;
        set(Phase.FAILED);
    }

    private void set(Phase p) { phase = p; rev++; }

    private static String lockedOrNot(VaultException e) {
        return e.reason() == VaultException.Reason.LOCKED ? "The vault is locked." : "That entry cannot be typed.";
    }

    /** User-facing text for a desktop error code; none of it comes from the desktop's free-text message. */
    public static String messageFor(String code) {
        switch (code == null ? "" : code) {
            case "target-changed": return "The desktop window changed, so nothing more was typed. Try again.";
            case "locked": return "The desktop is locked.";
            case "unverified": return "The desktop cannot tell which window has focus.";
            case "busy": return "The desktop is busy typing.";
            case "denied": return "Remote input was not approved on the desktop.";
            case "bad-request": return "The desktop refused this request.";
            case "disconnected": return "The desktop disconnected.";
            case "not-connected": return "The desktop is not connected.";
            default: return "The desktop could not type this.";
        }
    }
}
