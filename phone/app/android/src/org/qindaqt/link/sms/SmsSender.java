package org.qindaqt.link.sms;

import java.util.List;

/**
 * Sends text messages. The Android implementation uses SmsManager; tests use a fake.
 *
 * AGENT-CONTRACT: {@link Listener#onEvent} may be called from any thread, zero or more times per ref, with
 * state sent, delivered or failed; after failed or delivered the sender sends nothing more for that ref.
 */
public interface SmsSender {
    interface Listener {
        void onEvent(long ref, String state, String code);

        /**
         * A failure while handing recipients to the radio, after earlier recipients were already sent:
         * {@code unsent} lists the 0-based indexes of the addresses that were never sent. Terminal for the ref.
         */
        default void onUnsent(long ref, String code, java.util.List<Integer> unsent) { onEvent(ref, "failed", code); }
    }

    /** Starts sending. Throws (before anything is sent) IllegalArgumentException on input it cannot send, or another RuntimeException when the platform cannot send at all. Must not block on the network. */
    void send(long ref, List<String> addrs, String text, int subscription, Listener listener);

    /** Releases platform resources (receivers). The sender must not be used afterwards. */
    default void close() { }
}
