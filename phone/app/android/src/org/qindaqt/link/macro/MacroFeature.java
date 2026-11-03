package org.qindaqt.link.macro;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.engine.FeatureHandler;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.Json;

/**
 * The phone's {@code macro} capability: shows the desktop's macro library (id and name only) and asks
 * the desktop to run one. Macros run only while remote input is active; the macros screen starts it via
 * {@code InputFeature.begin()}.
 *
 * AGENT-CONTRACT: desktop side is src/services/link_macros macro_feature.cpp; {@code macro.state} is the
 * truth for running/done/cancelled/failed because ok/error replies do not reach handlers.
 * AGENT-GUARD: list entries are validated against the protocol bounds, so a hostile or buggy desktop cannot
 * hand the UI more than 64 rows of at most 80 characters.
 */
public final class MacroFeature implements FeatureHandler {
    static final int MAX_MACROS = 64;
    static final int MAX_NAME = 80;
    private static final Pattern ID = Pattern.compile("[a-zA-Z0-9_-]{1,40}");

    /** One library entry. */
    public static final class Entry {
        public final String id, name;
        Entry(String id, String name) { this.id = id; this.name = name; }
    }

    private final Object lock = new Object();
    private FeatureContext ctx;
    private volatile boolean connected;
    private List<Entry> macros = new ArrayList<>();
    private String rev = "";
    private String running = "", runState = "", reason = "", last = "";
    private String wantRun;
    private boolean wantCancel;
    private long runSince;
    static final long RUN_TIMEOUT_MS = 130_000; // longest legal macro plus slack

    @Override public String capability() { return "macro"; }
    @Override public List<String> namespaces() { return Collections.singletonList("macro"); }

    // ---- UI thread -----------------------------------------------------------------------------

    /** Asks the desktop to run the macro. Ignored while one is running or when not connected. */
    public void run(String id) {
        synchronized (lock) {
            if (!connected || !running.isEmpty() || id == null || !ID.matcher(id).matches()) return;
            wantRun = id;
            running = id;
            runState = "running";
            reason = "";
            runSince = System.nanoTime() / 1_000_000L;
        }
        wake();
    }

    public void cancel() {
        synchronized (lock) { if (!running.isEmpty()) wantCancel = true; }
        wake();
    }

    public List<Entry> macros() { synchronized (lock) { return new ArrayList<>(macros); } }

    /** {@code {"connected":true,"macros":[{"id","name"}],"running":"","runState":"","reason":"","last":""}}. */
    public String json() {
        synchronized (lock) {
            Map<String, Object> m = new LinkedHashMap<>();
            m.put("connected", connected);
            List<Object> rows = new ArrayList<>();
            for (Entry e : macros) {
                Map<String, Object> r = new LinkedHashMap<>();
                r.put("id", e.id);
                r.put("name", e.name);
                rows.add(r);
            }
            m.put("macros", rows);
            m.put("running", running);
            m.put("runState", runState);
            m.put("reason", reason);
            m.put("last", last);
            return Json.write(m);
        }
    }

    // ---- session thread ------------------------------------------------------------------------

    @Override public void onReady(FeatureContext c) throws IOException {
        ctx = c;
        connected = true;
        synchronized (lock) { running = runState = reason = ""; wantRun = null; wantCancel = false; }
    }

    @Override public void onTick(long nowMs) throws IOException {
        FeatureContext c = ctx;
        if (c == null || !c.peerHas("macro")) return;
        String run;
        boolean cancel;
        synchronized (lock) {
            if (!running.isEmpty() && System.nanoTime() / 1_000_000L - runSince > RUN_TIMEOUT_MS) {
                running = ""; runState = "failed"; reason = "timeout";
            }
        }
        synchronized (lock) { run = wantRun; cancel = wantCancel; wantRun = null; wantCancel = false; }
        if (run != null) {
            Map<String, Object> b = new LinkedHashMap<>();
            b.put("id", run);
            c.send(new Envelope("macro.run", c.nextId(), 0, b));
        }
        if (cancel) c.send(new Envelope("macro.cancel", c.nextId(), 0, null));
    }

    @Override @SuppressWarnings("unchecked") public boolean onFrame(Envelope e) throws IOException {
        if (e.t.equals("macro.list")) {
            Object rows = e.b.get("macros");
            if (!(rows instanceof List)) return true;
            List<Entry> next = new ArrayList<>();
            for (Object o : (List<Object>) rows) {
                if (next.size() >= MAX_MACROS) break;
                if (!(o instanceof Map)) continue;
                Object id = ((Map<String, Object>) o).get("id"), name = ((Map<String, Object>) o).get("name");
                if (!(id instanceof String) || !(name instanceof String) || !ID.matcher((String) id).matches()) continue;
                String n = (String) name;
                if (n.isEmpty() || n.length() > MAX_NAME) continue;
                next.add(new Entry((String) id, n));
            }
            Object r = e.b.get("rev");
            synchronized (lock) { macros = next; rev = r instanceof String ? (String) r : ""; }
            return true;
        }
        if (e.t.equals("macro.state")) {
            Object id = e.b.get("id"), st = e.b.get("state"), why = e.b.get("reason");
            if (!(id instanceof String) || !(st instanceof String)) return true;
            synchronized (lock) {
                String s = (String) st;
                if (!s.equals("running") && !s.equals("done") && !s.equals("cancelled") && !s.equals("failed")) return true;
                running = s.equals("running") ? (String) id : "";
                last = (String) id;
                runState = s;
                reason = why instanceof String && ((String) why).length() <= 40 ? (String) why : "";
            }
            return true;
        }
        return false;
    }

    @Override public void onClosed() {
        connected = false;
        ctx = null;
        synchronized (lock) { running = ""; runState = ""; wantRun = null; wantCancel = false; }
    }

    private void wake() {
        FeatureContext c = ctx;
        if (c != null) c.wake();
    }
}
