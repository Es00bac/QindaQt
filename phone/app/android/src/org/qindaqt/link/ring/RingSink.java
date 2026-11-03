package org.qindaqt.link.ring;

/**
 * Where a {@code ring} actually becomes sound and a notification. The Android implementation lives in
 * org.qindaqt.link.android; tests pass a fake so nothing ever rings.
 *
 * AGENT-CONTRACT: {@link #start} and {@link #stop} are called on the session thread only; the sink calls
 * the {@code onStopped} callback it was given from any thread, at most once per start, when the user ended
 * the ring (Stop action, unlock, opening the app).
 */
public interface RingSink {
    /**
     * Starts the alarm. Returns a short capability note for engine state, for example {@code "alarm"} or
     * {@code "alarm,no-dnd-access"}; never user content. Throws {@link IllegalStateException} if it
     * cannot ring at all.
     */
    String start(Runnable onStopped);

    /** Stops the alarm and restores the previous alarm volume. Idempotent. */
    void stop();
}
