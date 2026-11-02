package org.qindaqt.link.android;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.telephony.SmsManager;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.sms.SendTracker;
import org.qindaqt.link.sms.SmsSender;

/**
 * Sends through SmsManager (multipart) and folds the per-part sent/delivered broadcasts into
 * whole-request events via {@link SendTracker}.
 *
 * AGENT-NOTE: QindaLink is not the default SMS app, so the platform does not write the sent message to the
 * provider. Never call this from a test against the real phone; the live send waits for explicit user approval.
 */
public final class SmsSenderAndroid implements SmsSender {
    private static final String ACTION = "org.qindaqt.link.SMS_RESULT";
    private static final int KEEP = 32;
    private final Context ctx;
    private final Map<Long, SendTracker> trackers = new LinkedHashMap<>();
    private int requestCodes;

    public SmsSenderAndroid(Context ctx) {
        this.ctx = ctx.getApplicationContext();
        receiver = new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) { onResult(i, getResultCode()); }
        };
        this.ctx.registerReceiver(receiver, new IntentFilter(ACTION), Context.RECEIVER_NOT_EXPORTED);
    }

    @Override public void send(long ref, List<String> addrs, String text, int sub, Listener listener) {
        SmsManager sm = sub >= 0 ? SmsManager.getSmsManagerForSubscriptionId(sub) : ctx.getSystemService(SmsManager.class);
        if (sm == null) throw new IllegalStateException("no SmsManager");
        ArrayList<String> parts = sm.divideMessage(text);
        if (parts == null || parts.isEmpty()) throw new IllegalArgumentException("empty message");
        Listener wrapped = new Listener() {
            @Override public void onEvent(long r, String state, String code) {
                if (!"sent".equals(state)) forget(r);
                listener.onEvent(r, state, code);
            }
            @Override public void onUnsent(long r, String code, List<Integer> unsent) {
                forget(r);
                listener.onUnsent(r, code, unsent);
            }
        };
        SendTracker tracker = new SendTracker(ref, parts.size() * addrs.size(), wrapped);
        synchronized (this) {
            trackers.put(ref, tracker);
            while (trackers.size() > KEEP) trackers.remove(trackers.keySet().iterator().next());
        }
        // AGENT-GUARD: if recipient i fails, recipients before i are already on the air. Before the first one
        // the exception propagates (nothing sent, the caller may retry). After it, the request was accepted, so
        // report which recipients were never sent instead of throwing, which would invite a duplicate resend.
        for (int i = 0; i < addrs.size(); i++) {
            try {
                ArrayList<PendingIntent> sent = new ArrayList<>(), dlv = new ArrayList<>();
                for (int p = 0; p < parts.size(); p++) { sent.add(pi(ref, "sent")); dlv.add(pi(ref, "dlv")); }
                sm.sendMultipartTextMessage(addrs.get(i), null, parts, sent, dlv);
            } catch (RuntimeException ex) {
                forget(ref);
                if (i == 0) throw ex;
                List<Integer> unsent = new ArrayList<>();
                for (int j = i; j < addrs.size(); j++) unsent.add(j);
                listener.onUnsent(ref, ex instanceof SecurityException ? "denied" : "unknown", unsent);
                return;
            }
        }
    }

    private synchronized void forget(long ref) { trackers.remove(ref); }

    private BroadcastReceiver receiver;

    @Override public synchronized void close() {
        if (receiver != null) {
            try { ctx.unregisterReceiver(receiver); } catch (IllegalArgumentException e) { /* already gone */ }
            receiver = null;
        }
        trackers.clear();
    }

    private synchronized PendingIntent pi(long ref, String kind) {
        Intent i = new Intent(ACTION).setPackage(ctx.getPackageName()).putExtra("ref", ref).putExtra("kind", kind);
        return PendingIntent.getBroadcast(ctx, requestCodes++, i, PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_ONE_SHOT);
    }

    private void onResult(Intent i, int result) {
        SendTracker t;
        synchronized (this) { t = trackers.get(i.getLongExtra("ref", 0)); }
        if (t == null) return;
        if ("sent".equals(i.getStringExtra("kind"))) t.partSent(result == Activity.RESULT_OK ? null : code(result));
        else t.partDelivered(result == Activity.RESULT_OK);
    }

    private static String code(int r) {
        switch (r) {
            case SmsManager.RESULT_ERROR_GENERIC_FAILURE: return "generic";
            case SmsManager.RESULT_ERROR_NO_SERVICE: return "no-service";
            case SmsManager.RESULT_ERROR_NULL_PDU: return "null-pdu";
            case SmsManager.RESULT_ERROR_RADIO_OFF: return "radio-off";
            case SmsManager.RESULT_ERROR_LIMIT_EXCEEDED: return "limit";
            default: return "unknown";
        }
    }
}
