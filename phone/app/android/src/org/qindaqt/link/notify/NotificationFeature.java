package org.qindaqt.link.notify;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.engine.FeatureHandler;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.Json;

/**
 * The `notify` capability (phone side): mirrors notifications to the desktop and executes
 * dismissals and actions the desktop asks for. Wire format: protocol page, "Notifications".
 *
 * Threading: source callbacks land in a coalescing queue from any thread and the session
 * thread drains it in {@link #onTick} (at most 20 frames per tick), so a burst of updates to
 * one key becomes one frame and the writer is never flooded.
 *
 * AGENT-GUARD: titles, texts and reply texts are personal content. Never log them, not even at
 * debug level; log the frame type and sizes only.
 * AGENT-CONTRACT: mirrors src/services/link_notifications/NotificationFeature on the desktop.
 */
public final class NotificationFeature implements FeatureHandler {
    private static final int MAX_FRAMES_PER_TICK = 20;
    private static final int MAX_KNOWN = 400;

    private final NotificationSource source;
    private final EngineState state;
    private volatile FeatureContext ctx;
    /** key -> posted notification, or null for "removed"; insertion ordered, latest wins. */
    private final Map<String, PhoneNotification> queue = new LinkedHashMap<>();
    private boolean resnapshot;
    /** key -> JSON of the last frame sent, to drop no-op updates. */
    private final Map<String, String> sent = new LinkedHashMap<>();

    public NotificationFeature(NotificationSource source, EngineState state) {
        this.source = source;
        this.state = state;
    }

    @Override public String capability() { return "notify"; }
    @Override public List<String> namespaces() { return Collections.singletonList("notify"); }

    @Override public void onReady(FeatureContext c) throws IOException {
        ctx = c;
        synchronized (queue) { queue.clear(); resnapshot = false; }
        sent.clear();
        state.setNotifyAccess(source.accessGranted());
        source.setListener(new NotificationSource.Listener() {
            @Override public void onPosted(PhoneNotification n) { enqueue(n.key, n); }
            @Override public void onRemoved(String key) { enqueue(key, null); }
            @Override public void onAccessChanged(boolean granted) {
                state.setNotifyAccess(granted);
                if (granted) synchronized (queue) { resnapshot = true; }
                FeatureContext cc = ctx;
                if (cc != null) cc.wake();
            }
        });
        sendSnapshot();
    }

    private void enqueue(String key, PhoneNotification n) {
        synchronized (queue) {
            queue.remove(key);  // re-insert so ordering follows the latest event
            queue.put(key, n);
        }
        FeatureContext c = ctx;
        if (c != null) c.wake();
    }

    private void sendSnapshot() throws IOException {
        if (!ctx.peerHas("notify")) return;
        List<PhoneNotification> items = new ArrayList<>(source.active());
        Collections.sort(items, new Comparator<PhoneNotification>() {
            @Override public int compare(PhoneNotification a, PhoneNotification b) { return Long.compare(b.when, a.when); }
        });
        if (items.size() > NotifyFrames.SNAPSHOT_ITEMS) items = items.subList(0, NotifyFrames.SNAPSHOT_ITEMS);
        Envelope e = NotifyFrames.snapshot(items);
        if (!ctx.send(e)) return;
        sent.clear();
        for (PhoneNotification n : items) remember(n.key, Json.write(NotifyFrames.postedBody(n)));
    }

    private void remember(String key, String json) {
        sent.remove(key);
        sent.put(key, json);
        if (sent.size() > MAX_KNOWN) sent.remove(sent.keySet().iterator().next());
    }

    @Override public void onTick(long nowMs) throws IOException {
        if (ctx == null || !ctx.peerHas("notify")) return;
        boolean again;
        synchronized (queue) { again = resnapshot; resnapshot = false; }
        if (again) {
            synchronized (queue) { queue.clear(); }
            sendSnapshot();
            return;
        }
        for (int i = 0; i < MAX_FRAMES_PER_TICK; i++) {
            String key;
            PhoneNotification n;
            synchronized (queue) {
                if (queue.isEmpty()) return;
                key = queue.keySet().iterator().next();
                n = queue.remove(key);
            }
            if (n == null) {
                // A removal for a key the desktop never saw (filtered, or coalesced with its post) is silent.
                if (sent.remove(key) != null) ctx.send(NotifyFrames.removed(key));
                continue;
            }
            String json = Json.write(NotifyFrames.postedBody(n));
            if (json.equals(sent.get(key))) continue;
            if (ctx.send(NotifyFrames.posted(n))) remember(key, json);
        }
        boolean more;
        synchronized (queue) { more = !queue.isEmpty(); }
        if (more) ctx.wake();
    }

    @Override public boolean onFrame(Envelope e) throws IOException {
        switch (e.t) {
            case "notify.dismiss": dismiss(e); return true;
            case "notify.action": action(e); return true;
            case "notify.icon": icon(e); return true;
            default: return false;  // notify.posted/removed/snapshot flow phone to desktop only
        }
    }

    private void dismiss(Envelope e) throws IOException {
        String key = NotifyFrames.field(e.b, "key", NotifyFrames.KEY);
        if (key == null) { fail(e, "bad-request"); return; }
        if (!source.accessGranted()) { fail(e, "denied"); return; }
        if (!source.dismiss(key)) { fail(e, "target-changed"); return; }
        if (e.id != 0) ctx.replyOk(e.id);
    }

    private void action(Envelope e) throws IOException {
        String key = NotifyFrames.field(e.b, "key", NotifyFrames.KEY);
        String action = NotifyFrames.field(e.b, "action", NotifyFrames.ACTION_ID);
        String title = NotifyFrames.field(e.b, "title", NotifyFrames.ACTION_TITLE);
        boolean[] ok = new boolean[1];
        String text = NotifyFrames.replyText(e.b, ok);
        if (key == null || action == null || title == null || !ok[0]) { fail(e, "bad-request"); return; }
        if (!source.accessGranted()) { fail(e, "denied"); return; }
        String err = source.perform(key, action, title, text);
        if ("unverified".equals(err)) {
            if (e.id != 0) ctx.replyError(e.id, "unsupported", "unverified");
            return;
        }
        if (err != null) { fail(e, err); return; }
        if (e.id != 0) ctx.replyOk(e.id);
    }

    private void icon(Envelope e) throws IOException {
        Object h = e.b.get("hash");
        if (!(h instanceof String) || !NotifyFrames.isHash((String) h)) { fail(e, "bad-request"); return; }
        ctx.send(NotifyFrames.icon((String) h, source.icon((String) h)));
    }

    private void fail(Envelope e, String code) throws IOException {
        if (e.id != 0) ctx.replyError(e.id, code, "notify");
    }

    @Override public void onClosed() {
        source.setListener(null);
        ctx = null;
    }
}
