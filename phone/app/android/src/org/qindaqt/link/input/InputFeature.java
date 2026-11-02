package org.qindaqt.link.input;

import java.io.IOException;
import java.util.ArrayDeque;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.engine.FeatureHandler;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.Json;

/**
 * The phone's {@code input} capability: touchpad, keys and text to the desktop.
 *
 * The UI thread calls begin()/end()/touch()/sendKey()/sendText(); the session thread sends. Calls only
 * queue; {@link #onTick} (woken immediately) sends, so FeatureContext is used on the session thread only.
 * The desktop's {@code input.state} frames are the truth for the UI: the engine does not deliver
 * ok/error replies to handlers (protocol page, "State frames carry the UI state").
 *
 * AGENT-GUARD: nothing is sent unless the state is ACTIVE, except the begin and end commands. end()
 * resets the gesture engine first so a held drag button is released before input.end.
 * AGENT-CONTRACT: desktop side is src/services/link_input input_feature.cpp.
 */
public final class InputFeature implements FeatureHandler {
    /** What the UI shows. */
    public enum State { OFF, OFFLINE, PENDING, ACTIVE, DENIED, UNAVAILABLE, LOCKED, BUSY, REVOKED }

    static final int QUEUE_LIMIT = 512;
    static final long PENDING_TIMEOUT_MS = 150_000;
    static final int TEXT_CHUNK_BYTES = 1024;

    private final Object lock = new Object();
    private final ArrayDeque<Envelope> queue = new ArrayDeque<>();
    private final ArrayDeque<String> texts = new ArrayDeque<>();
    private final TouchGestures gestures;
    private final Clock clock;
    private FeatureContext ctx;           // session thread (set in onReady/onClosed)
    private volatile boolean connected;
    private State state = State.OFFLINE;  // guarded by lock
    private boolean wantInput, sendBegin, sendEnd; // guarded by lock
    private long pendingSince;
    private boolean hapticDue;

    /** Monotonic milliseconds; replaced in tests. */
    public interface Clock { long nowMs(); }

    public InputFeature() { this(() -> System.nanoTime() / 1_000_000L); }

    public InputFeature(Clock clock) {
        this.clock = clock;
        this.gestures = new TouchGestures(new TouchGestures.Sink() {
            @Override public void pointer(double dx, double dy) { enqueue(InputFrames.pointer(dx, dy), true); }
            @Override public void button(String b, String s) { enqueue(InputFrames.button(b, s), false); }
            @Override public void scroll(double dx, double dy, boolean end) { enqueue(InputFrames.scroll(dx, dy, end), true); }
            @Override public void haptic() { hapticDue = true; }
        });
    }

    @Override public String capability() { return "input"; }
    @Override public List<String> namespaces() { return Collections.singletonList("input"); }

    // ---- UI thread API -------------------------------------------------------------------------

    /** The screen was opened: ask the desktop for remote input (it may prompt its user once). */
    public void begin() {
        synchronized (lock) {
            wantInput = true;
            if (!connected) { state = State.OFFLINE; return; }
            if (state == State.ACTIVE || state == State.PENDING) return;
            sendBegin = true;
            state = State.PENDING;
            pendingSince = clock.nowMs();
        }
        wake();
    }

    /** The screen was left: release a held drag, then end remote input. */
    public void end() {
        gestures.reset();
        synchronized (lock) {
            wantInput = false;
            sendBegin = false;
            if (state == State.ACTIVE || state == State.PENDING || state == State.LOCKED) sendEnd = true;
            if (state != State.OFFLINE) state = connected ? State.OFF : State.OFFLINE;
            texts.clear();
        }
        wake();
    }

    public TouchGestures gestures() { return gestures; }

    /** One touch frame from the touchpad surface; returns true when the UI should buzz once. */
    public boolean touch(int fingers, double x0, double y0, double x1, double y1) {
        synchronized (lock) { if (state != State.ACTIVE) return false; }
        gestures.update(clock.nowMs(), fingers, x0, y0, x1, y1);
        boolean h = hapticDue;
        hapticDue = false;
        return h;
    }

    public void setSensitivity(double s) { gestures.setSensitivity(s); }

    /** Left/right/middle click button (the touchpad's explicit buttons). */
    public void click(String button) { enqueue(InputFrames.button(button, "click"), false); }

    /** A named key with latched modifiers (see KeyChord bit masks). */
    public void sendKey(String name, int mods) {
        for (Envelope e : KeyChord.frames(name, mods)) enqueue(e, false);
    }

    /** A single printable character with latched modifiers. */
    public void sendChar(int codePoint, int mods) {
        for (Envelope e : KeyChord.frames(codePoint, mods)) enqueue(e, false);
    }

    /** Text for the focused desktop window; chunked and paced so the desktop can type each chunk. */
    public void sendText(String text) {
        synchronized (lock) {
            if (state != State.ACTIVE) return;
            for (String chunk : InputFrames.splitText(text, TEXT_CHUNK_BYTES)) {
                if (texts.size() < 16) texts.add(chunk);
            }
        }
        wake();
    }

    public State state() { synchronized (lock) { return state; } }

    /** {@code {"state":"active"}} for the bridge; keys are stable. */
    public String json() {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("state", state().name().toLowerCase());
        return Json.write(m);
    }

    // ---- session thread ------------------------------------------------------------------------

    @Override public void onReady(FeatureContext c) throws IOException {
        ctx = c;
        connected = true;
        synchronized (lock) {
            queue.clear();
            texts.clear();
            state = State.OFF;
            if (wantInput) { sendBegin = true; state = State.PENDING; pendingSince = clock.nowMs(); }
        }
        c.wake();
    }

    @Override public void onTick(long nowMs) throws IOException {
        FeatureContext c = ctx;
        if (c == null || !c.peerHas("input")) return;
        boolean begin, end;
        String text = null;
        Envelope[] frames;
        synchronized (lock) {
            begin = sendBegin;
            end = sendEnd;
            sendBegin = sendEnd = false;
            if (state == State.PENDING && clock.nowMs() - pendingSince > PENDING_TIMEOUT_MS) state = State.UNAVAILABLE;
            // AGENT-GUARD: frames queued before end() (the release of a held drag) still go out ahead of input.end.
            frames = state == State.ACTIVE || end ? queue.toArray(new Envelope[0]) : new Envelope[0];
            queue.clear();
            if (state == State.ACTIVE && !texts.isEmpty()) text = texts.poll();
        }
        if (begin) c.send(new Envelope("input.begin", c.nextId(), 0, null));
        for (Envelope e : frames) c.send(e);
        if (text != null) c.send(InputFrames.text(text));
        if (end) c.send(new Envelope("input.end", 0, 0, null));
        synchronized (lock) { if (!texts.isEmpty()) c.wake(); }
    }

    @Override public boolean onFrame(Envelope e) throws IOException {
        if (e.t.equals("input.state")) {
            Object s = e.b.get("state");
            synchronized (lock) {
                State next = parse(s instanceof String ? (String) s : "");
                if (next != null && wantInput) {
                    state = next;
                    if (next != State.ACTIVE) queue.clear();
                }
            }
            return true;
        }
        if (e.t.equals("input.end")) { // revoked on the desktop
            gestures.reset();
            synchronized (lock) { if (wantInput) state = State.REVOKED; queue.clear(); texts.clear(); }
            return true;
        }
        return false;
    }

    @Override public void onClosed() {
        connected = false;
        ctx = null;
        gestures.reset();
        synchronized (lock) { state = State.OFFLINE; queue.clear(); texts.clear(); sendBegin = sendEnd = false; }
    }

    static State parse(String s) {
        switch (s) {
            case "pending": return State.PENDING;
            case "active": return State.ACTIVE;
            case "denied": return State.DENIED;
            case "unavailable": return State.UNAVAILABLE;
            case "locked": return State.LOCKED;
            case "busy": return State.BUSY;
            default: return null;
        }
    }

    private void enqueue(Envelope e, boolean droppable) {
        if (e == null) return;
        synchronized (lock) {
            if (state != State.ACTIVE) return;
            if (queue.size() >= QUEUE_LIMIT) {
                if (droppable) return;
                queue.pollFirst(); // keep releases and clicks over stale movement
            }
            queue.add(e);
        }
        wake();
    }

    private void wake() {
        FeatureContext c = ctx;
        if (c != null) c.wake();
    }
}
