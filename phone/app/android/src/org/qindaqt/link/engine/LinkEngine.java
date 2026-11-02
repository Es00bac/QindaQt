package org.qindaqt.link.engine;

import java.io.IOException;

/**
 * Connection loop: connect to the desktop, run a {@link Session}, back off,
 * repeat. Owns one daemon thread. Android-free; the service supplies the
 * {@link Connector}. Survives without any UI.
 *
 * AGENT-GUARD: {@link #start} and {@link #stop} are idempotent. UI calls reach
 * the live session only through {@link Session#command}.
 */
public final class LinkEngine {
    /** Opens a handshaken Link or throws. Blocking. */
    public interface Connector {
        Link connect() throws IOException;
    }

    private static final long[] BACKOFF_MS = {1000, 2000, 4000, 8000, 15000};

    private final Connector connector;
    private final PinStore pins;
    private final FeatureRegistry features;
    private final DeviceInfo info;
    private final EngineState state;
    private volatile Session session;
    private volatile boolean running;
    private Thread thread;

    public LinkEngine(Connector c, PinStore pins, FeatureRegistry features, DeviceInfo info, EngineState state) {
        this.connector = c;
        this.pins = pins;
        this.features = features;
        this.state = state;
        this.info = info;
        state.setPaired(pins.deviceId() != null, pins.name());
    }

    public EngineState state() { return state; }

    public synchronized void start() {
        if (running) return;
        running = true;
        thread = new Thread(this::loop, "qindalink-engine");
        thread.setDaemon(true);
        thread.start();
    }

    public synchronized void stop() {
        running = false;
        Session s = session;
        if (s != null) s.command(Session.Command.STOP);
        if (thread != null) thread.interrupt();
        thread = null;
    }

    /** UI action; ignored when no session is live. */
    public void command(Session.Command c) {
        Session s = session;
        if (s != null) s.command(c);
    }

    /** Forget the paired desktop. The next session starts unpaired. */
    public void forget() {
        pins.clear();
        state.setPaired(false, null);
        Session s = session;
        if (s != null) s.command(Session.Command.STOP);
    }

    private void loop() {
        int failures = 0;
        while (running) {
            Link link = null;
            try {
                if (state.phase() != EngineState.Phase.ERROR) state.set(EngineState.Phase.CONNECTING, "", "", "Connecting");
                link = connector.connect();
                Session s = new Session(link, pins, features, info, state);
                session = s;
                long t0 = System.currentTimeMillis();
                s.run();
                session = null;
                failures = System.currentTimeMillis() - t0 > 20000 ? 0 : failures + 1;
            } catch (IOException e) {
                session = null;
                if (state.phase() != EngineState.Phase.ERROR) state.set(EngineState.Phase.DISCONNECTED, "", "", "No desktop reachable");
                failures++;
            } catch (RuntimeException e) {
                session = null;
                state.set(EngineState.Phase.ERROR, "", "", "Internal error: " + e.getClass().getSimpleName());
                failures++;
            }
            try {
                Thread.sleep(BACKOFF_MS[Math.min(Math.max(failures - 1, 0), BACKOFF_MS.length - 1)]);
            } catch (InterruptedException e) {
                return;
            }
        }
    }
}
