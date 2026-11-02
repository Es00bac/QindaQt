package org.qindaqt.link.sms;

import java.util.List;

/**
 * Sends text messages. The Android implementation uses SmsManager; tests use a fake.
 *
 * AGENT-CONTRACT: {@link Listener#onEvent} may be called from any thread, zero or more times per ref, with
 * state sent, delivered or failed; after failed or delivered the sender sends nothing more for that ref.
 */
public interface SmsSender {
    interface Listener { void onEvent(long ref, String state, String code); }

    /** Starts sending. Throws IllegalArgumentException on input it cannot send. Must not block on the network. */
    void send(long ref, List<String> addrs, String text, int subscription, Listener listener);
}
