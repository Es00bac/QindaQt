package org.qindaqt.link.ring;

import java.io.IOException;
import java.util.Collections;
import java.util.List;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.engine.FeatureHandler;
import org.qindaqt.link.proto.Envelope;

/**
 * The phone's {@code ring} capability: desktop asks the phone to sound a loud alarm so it can be found.
 * {@code ring} starts it (reply ok, or error{busy} if already ringing); {@code ring{stop:true}} stops it
 * (ok). It also stops on a user action reported by the sink, after {@link #MAX_RING_MS}, and when the
 * session ends.
 *
 * AGENT-CONTRACT: desktop side is src/services/link_transport RingFeature (protocol page, Core frames).
 * AGENT-GUARD: the 60 s cap is counted on the session tick (about 500 ms), so a ring can overrun by one
 * tick. Stopping on {@link #onClosed} is deliberate: a dead link must not leave a phone ringing that
 * nobody can silence from the desktop.
 */
public final class RingFeature implements FeatureHandler {
    public static final long MAX_RING_MS = 60_000;

    private final RingSink sink;
    private final EngineState state;
    private final long maxMs;
    private FeatureContext ctx;
    private boolean ringing;
    private long startedMs;
    private volatile boolean userStopped;

    public RingFeature(RingSink sink, EngineState state) { this(sink, state, MAX_RING_MS); }

    /** Test seam: a shorter cap than the protocol's 60 s. */
    public RingFeature(RingSink sink, EngineState state, long maxMs) {
        this.sink = sink;
        this.state = state;
        this.maxMs = maxMs;
    }

    @Override public String capability() { return "ring"; }
    @Override public List<String> namespaces() { return Collections.singletonList("ring"); }

    @Override public void onReady(FeatureContext c) {
        ctx = c;
        ringing = false;
        userStopped = false;
        state.setRing("idle");
    }

    @Override public boolean onFrame(Envelope e) throws IOException {
        if (!e.t.equals("ring")) return false;
        if (e.id == 0) return true; // a ring is a request; without an id there is nothing to answer
        if (Boolean.TRUE.equals(e.b.get("stop"))) {
            finish();
            ctx.replyOk(e.id);
            return true;
        }
        if (ringing) {
            ctx.replyError(e.id, "busy", "already ringing");
            return true;
        }
        try {
            final FeatureContext c = ctx;
            String note = sink.start(() -> { userStopped = true; c.wake(); });
            ringing = true;
            userStopped = false;
            startedMs = System.nanoTime() / 1_000_000L;
            state.setRing("ringing," + note);
            ctx.replyOk(e.id);
        } catch (RuntimeException ex) {
            state.setRing("failed");
            ctx.replyError(e.id, "internal", "cannot ring");
        }
        return true;
    }

    @Override public void onTick(long nowMs) {
        if (!ringing) return;
        if (userStopped || System.nanoTime() / 1_000_000L - startedMs >= maxMs) finish();
    }

    @Override public void onClosed() {
        finish();
        ctx = null;
    }

    private void finish() {
        if (!ringing) return;
        ringing = false;
        sink.stop();
        state.setRing("idle");
    }
}
