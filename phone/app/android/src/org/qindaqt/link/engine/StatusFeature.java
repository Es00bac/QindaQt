package org.qindaqt.link.engine;

import java.io.IOException;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.Json;

/**
 * The `status` capability: battery and network facts, phone to desktop only.
 * Sent once on READY, then on change, at most one per 2 s unless the
 * charging/plugged pair changed (protocol page, Status).
 */
public final class StatusFeature implements FeatureHandler {
    private static final long MIN_GAP_MS = 2000;

    private final StatusSource source;
    private final EngineState state;
    private FeatureContext ctx;
    private long lastMs;
    private String lastJson = "", lastChargeKey = "";

    public StatusFeature(StatusSource source, EngineState state) {
        this.source = source;
        this.state = state;
    }

    @Override public String capability() { return "status"; }
    @Override public List<String> namespaces() { return Collections.singletonList("status"); }

    @Override public void onReady(FeatureContext c) throws IOException {
        ctx = c;
        lastJson = lastChargeKey = "";
        source.setListener(c::wake);
        send(0, true);
    }

    @Override public void onTick(long nowMs) throws IOException { if (ctx != null) send(nowMs, false); }

    @Override public boolean onFrame(Envelope e) { return false; } // phone to desktop only

    @Override public void onClosed() {
        source.setListener(null);
        ctx = null;
    }

    private void send(long t, boolean force) throws IOException {
        if (!ctx.peerHas("status")) return;
        Map<String, Object> body = source.snapshot();
        String json = Json.write(body);
        if (!force && json.equals(lastJson)) return;
        String key = chargeKey(body);
        boolean urgent = !key.equals(lastChargeKey);
        if (!force && !urgent && t - lastMs < MIN_GAP_MS) return;
        if (!ctx.send(Envelope.of("status", body))) return;
        lastJson = json;
        lastChargeKey = key;
        lastMs = t;
        state.statusSent(System.currentTimeMillis());
    }

    @SuppressWarnings("unchecked")
    private static String chargeKey(Map<String, Object> body) {
        Object b = body.get("battery");
        if (!(b instanceof Map)) return "";
        Map<String, Object> m = (Map<String, Object>) b;
        return m.get("charging") + "/" + m.get("plugged");
    }
}
