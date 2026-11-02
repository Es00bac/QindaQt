package org.qindaqt.link.sms;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicLong;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.engine.FeatureHandler;
import org.qindaqt.link.proto.Envelope;

/**
 * The `sms` capability (protocol page, Messages): serves thread and message pages, MMS parts, send and
 * mark-read requests from the desktop, and pushes sms.changed / sms.sent events.
 *
 * AGENT-CONTRACT: mirrors QindaQt::Link::MessagesFeature on the desktop. Message bodies, addresses and names
 * pass through here but are never logged, and EngineState gets only a permission summary.
 * AGENT-NOTE: provider and sender callbacks arrive on arbitrary threads; they only touch the two queues and
 * call ctx.wake(). All frame sending happens in onTick/onFrame on the session thread.
 */
public final class SmsFeature implements FeatureHandler {
    static final long DEBOUNCE_MS = 500;
    static final int MAX_PART_BYTES = 2 * 1024 * 1024, MAX_TEXT = 1600, MAX_SEND_ADDRS = 10;

    private final SmsProvider provider;
    private final SmsModel model;
    private final SmsSender sender;
    private final EngineState state;
    private final AtomicLong refs = new AtomicLong();
    private final ConcurrentLinkedQueue<Object[]> sentEvents = new ConcurrentLinkedQueue<>();
    private volatile long dirtyAtMs; // 0 = clean
    private FeatureContext ctx;
    private Map<Long, String> known = new HashMap<>();

    public SmsFeature(SmsProvider provider, ContactResolver contacts, SmsSender sender, EngineState state) {
        this.provider = provider;
        this.model = new SmsModel(provider, contacts);
        this.sender = sender;
        this.state = state;
    }

    @Override public String capability() { return "sms"; }
    @Override public List<String> namespaces() { return Collections.singletonList("sms"); }

    @Override public void onReady(FeatureContext c) throws IOException {
        ctx = c;
        sentEvents.clear();
        known = safeFingerprints();
        dirtyAtMs = 0;
        provider.setChangeListener(() -> {
            if (dirtyAtMs == 0) dirtyAtMs = System.currentTimeMillis();
            FeatureContext x = ctx;
            if (x != null) x.wake();
        });
        report();
        sendChanged(new ArrayList<Long>(), false); // lets the desktop learn the unread total
    }

    @Override public void onTick(long nowMs) throws IOException {
        if (ctx == null) return;
        for (Object[] e; (e = sentEvents.poll()) != null; ) {
            Map<String, Object> b = new LinkedHashMap<>();
            b.put("ref", e[0]);
            b.put("state", e[1]);
            if (e[2] != null) b.put("code", e[2]);
            if (e[3] != null) b.put("unsent", e[3]);
            ctx.send(Envelope.of("sms.sent", b));
        }
        long d = dirtyAtMs;
        if (d != 0 && System.currentTimeMillis() - d >= DEBOUNCE_MS) {
            dirtyAtMs = 0;
            diffAndNotify();
        } else if (d != 0) {
            ctx.wake();
        }
    }

    private void diffAndNotify() throws IOException {
        if (!provider.canRead()) return;
        Map<Long, String> now = safeFingerprints();
        if (now == null) return; // provider failed; the next change retries and the failure is logged by the provider
        List<Long> changed = new ArrayList<>();
        for (Map.Entry<Long, String> en : now.entrySet()) {
            if (!en.getValue().equals(known.get(en.getKey()))) changed.add(en.getKey());
        }
        for (Long id : known.keySet()) if (!now.containsKey(id)) changed.add(id);
        known = now;
        if (!changed.isEmpty()) sendChanged(changed, false);
    }

    private Map<Long, String> safeFingerprints() {
        if (!provider.canRead()) return new HashMap<Long, String>();
        try { return provider.fingerprints(); } catch (RuntimeException ex) { return null; }
    }

    private void sendChanged(List<Long> ids, boolean all) throws IOException {
        boolean readable = provider.canRead();
        Map<String, Object> b = new LinkedHashMap<>();
        boolean over = ids.size() > SmsModel.MAX_THREADS;
        b.put("threads", new ArrayList<Object>(over ? ids.subList(0, SmsModel.MAX_THREADS) : ids));
        b.put("all", all || over);
        b.put("unread", readable ? (long) provider.unreadTotal() : 0L);
        ctx.send(Envelope.of("sms.changed", b));
    }

    private void report() { state.setSms((provider.canRead() ? "read" : "no-read") + "," + (provider.canSend() ? "send" : "no-send")); }

    @Override public boolean onFrame(Envelope e) throws IOException {
        if (e.id == 0) return false;
        report();
        try {
            return dispatch(e);
        } catch (SmsProvider.ProviderException ex) {
            ctx.replyError(e.id, "internal", ex.getMessage()); // reason holds class names only
        } catch (RuntimeException ex) {
            ctx.replyError(e.id, "internal", "provider failure: " + ex.getClass().getSimpleName());
        }
        return true;
    }

    private boolean dispatch(Envelope e) throws IOException {
        switch (e.t) {
            case "sms.threads": return threads(e);
            case "sms.messages": return messages(e);
            case "sms.part": return part(e);
            case "sms.send": return send(e);
            case "sms.read": return read(e);
            default: return false;
        }
    }

    private boolean threads(Envelope e) throws IOException {
        if (!denyUnlessRead(e)) return true;
        Long limit = optInt(e, "limit"), before = optInt(e, "before"), beforeId = optInt(e, "beforeId");
        if (bad(e, limit, before, beforeId)) return true;
        reply(e, model.threads(SmsModel.clamp(limit == null ? -1 : limit, SmsModel.MAX_THREADS), cursorDate(before), cursor(beforeId)));
        return true;
    }

    private boolean messages(Envelope e) throws IOException {
        if (!denyUnlessRead(e)) return true;
        Long thread = optInt(e, "thread"), limit = optInt(e, "limit"), before = optInt(e, "before"), beforeId = optInt(e, "beforeId");
        Object kind = e.b.get("beforeKind");
        if (bad(e, thread, limit, before, beforeId)) return true;
        if (thread == null || (kind != null && !"sms".equals(kind) && !"mms".equals(kind))) { ctx.replyError(e.id, "bad-request", "thread required or bad beforeKind"); return true; }
        reply(e, model.messages(thread, SmsModel.clamp(limit == null ? -1 : limit, SmsModel.MAX_MESSAGES), cursorDate(before), "mms".equals(kind), cursor(beforeId)));
        return true;
    }

    private boolean part(Envelope e) throws IOException {
        if (!denyUnlessRead(e)) return true;
        Long part = optInt(e, "part"), edge = optInt(e, "maxEdge");
        if (bad(e, part, edge)) return true;
        if (part == null) { ctx.replyError(e.id, "bad-request", "part required"); return true; }
        int maxEdge = (int) Math.max(64, Math.min(2048, edge == null ? 1280 : edge));
        try {
            SmsProvider.PartData d = provider.part(part, maxEdge, MAX_PART_BYTES);
            if (d == null) { ctx.replyError(e.id, "bad-request", "no such part"); return true; }
            Map<String, Object> b = new LinkedHashMap<>();
            b.put("part", part);
            b.put("mime", SmsModel.cut(d.mime, 128));
            b.put("size", (long) d.bytes.length);
            b.put("scaled", d.scaled);
            b.put("data", java.util.Base64.getEncoder().encodeToString(d.bytes));
            reply(e, b);
        } catch (SmsProvider.TooLarge t) {
            ctx.replyError(e.id, "unsupported", "part too large");
        }
        return true;
    }

    @SuppressWarnings("unchecked")
    private boolean send(Envelope e) throws IOException {
        if (!provider.canSend()) { ctx.replyError(e.id, "denied", "SEND_SMS not granted"); return true; }
        Object a = e.b.get("addrs"), t = e.b.get("text");
        Long sub = optInt(e, "sub");
        if (!(a instanceof List) || !(t instanceof String) || bad(e, sub)) { ctx.replyError(e.id, "bad-request", "addrs and text required"); return true; }
        List<Object> raw = (List<Object>) a;
        String text = (String) t;
        List<String> addrs = new ArrayList<>();
        boolean ok = !raw.isEmpty() && raw.size() <= MAX_SEND_ADDRS && !text.isEmpty() && text.length() <= MAX_TEXT;
        for (Object o : raw) {
            if (!(o instanceof String) || ((String) o).length() > 32) { ok = false; break; }
        }
        if (!ok) { ctx.replyError(e.id, "bad-request", "invalid addrs or text"); return true; }
        // AGENT-NOTE: every recipient is validated before any is sent, so a bad one never leaves a partial send.
        for (Object o : raw) {
            String n = Dialable.normalize((String) o);
            if (n == null) { ctx.replyError(e.id, "denied", "recipient is not a dialable number"); return true; }
            addrs.add(n);
        }
        long ref = refs.incrementAndGet();
        try {
            sender.send(ref, addrs, text, sub == null ? -1 : (int) (long) sub, new SmsSender.Listener() {
                @Override public void onEvent(long r, String st, String code) { push(r, st, code, null); }
                @Override public void onUnsent(long r, String code, List<Integer> unsent) { push(r, "failed", code, new ArrayList<Object>(unsent)); }
            });
        } catch (SecurityException ex) {
            ctx.replyError(e.id, "denied", "cannot send");
            return true;
        } catch (IllegalArgumentException ex) {
            ctx.replyError(e.id, "bad-request", "cannot send");
            return true;
        } catch (RuntimeException ex) {
            ctx.replyError(e.id, "internal", "cannot send"); // nothing was sent: the sender throws only before the first recipient
            return true;
        }
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("ref", ref);
        reply(e, b);
        return true;
    }

    private void push(long ref, String state, String code, List<Object> unsent) {
        sentEvents.add(new Object[] {ref, state, code, unsent});
        FeatureContext x = ctx;
        if (x != null) x.wake();
    }

    private boolean read(Envelope e) throws IOException {
        if (!denyUnlessRead(e)) return true;
        Long thread = optInt(e, "thread");
        if (bad(e, thread)) return true;
        if (thread == null) { ctx.replyError(e.id, "bad-request", "thread required"); return true; }
        boolean done;
        try { done = provider.markRead(thread); } catch (SecurityException ex) { done = false; }
        if (!done) { ctx.replyError(e.id, "unsupported", "not the default SMS app"); return true; }
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("thread", thread);
        b.put("unread", (long) provider.unreadTotal());
        reply(e, b);
        return true;
    }

    private boolean denyUnlessRead(Envelope e) throws IOException {
        if (provider.canRead()) return true;
        ctx.replyError(e.id, "denied", "READ_SMS not granted");
        return false;
    }

    private void reply(Envelope req, Map<String, Object> body) throws IOException {
        ctx.send(new Envelope(req.t, 0, req.id, body));
    }

    /** Absent -> null. A present non-integer is flagged through {@link #bad}. */
    private static final Long WRONG = Long.MIN_VALUE;
    private static Long optInt(Envelope e, String key) {
        Object v = e.b.get(key);
        if (v == null) return null;
        return v instanceof Long ? (Long) v : WRONG;
    }

    private boolean bad(Envelope e, Long... vals) throws IOException {
        for (Long v : vals) {
            if (v != null && v == WRONG.longValue()) { ctx.replyError(e.id, "bad-request", "wrong field type"); return true; }
        }
        return false;
    }

    /** Absent, zero or negative means "from the newest": a real date is never 0, and clients pass 0 for unset. */
    private static long cursorDate(Long v) { return v == null || v <= 0 ? -1 : v; }
    private static long cursor(Long v) { return v == null ? -1 : v; }

    @Override public void onClosed() {
        provider.setChangeListener(null);
        ctx = null;
    }
}
