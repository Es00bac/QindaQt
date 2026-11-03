package org.qindaqt.link.sms;

/**
 * Folds per-part SmsManager results into the whole-request events of sms.send: sent when all parts
 * were sent, delivered when all delivery reports arrived, failed on the first failure. Thread-safe.
 */
public final class SendTracker {
    private final long ref;
    private final int parts;
    private final SmsSender.Listener out;
    private int sent, delivered;
    private boolean done, sentReported;

    /** @param parts total SMS parts across all addresses (parts per address times addresses). */
    public SendTracker(long ref, int parts, SmsSender.Listener out) {
        this.ref = ref; this.parts = parts; this.out = out;
    }

    /** One part left the phone, or failed with {@code code} (non-null). */
    public void partSent(String code) {
        String state = null, c = null;
        synchronized (this) {
            if (done) return;
            if (code != null) { done = true; state = "failed"; c = code; }
            else if (++sent == parts && !sentReported) { sentReported = true; state = "sent"; }
        }
        if (state != null) out.onEvent(ref, state, c);
    }

    /** One delivery report arrived; {@code ok} false means the carrier reported failure. */
    public void partDelivered(boolean ok) {
        String state = null, c = null;
        synchronized (this) {
            if (done) return;
            if (!ok) { done = true; state = "failed"; c = "generic"; }
            else if (++delivered == parts) { done = true; state = "delivered"; }
        }
        if (state != null) out.onEvent(ref, state, c);
    }
}
