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
        /** "usb", "lan" or "none" for the link last returned by connect(). */
        default String transport() { return "usb"; }
        /** While on a non-USB link: true when USB can take over (the engine then ends the session). Blocking. */
        default boolean upgradeAvailable() { return false; }
        /** The session ended or no connection exists. */
        default void disconnected() { }
    }

    private static final long[] BACKOFF_MS = {1000, 2000, 4000, 8000, 15000};
    private static final long UPGRADE_CHECK_MS = 15000;

    private final Connector connector;
    private final PinStore pins;
    private final FeatureRegistry features;
    private final DeviceInfo info;
    private final EngineState state;
    private final Backoff backoff;
    private final long upgradeCheckMs;
    private volatile Session session;
    private volatile boolean running;
    private Thread thread;

    public LinkEngine(Connector c, PinStore pins, FeatureRegistry features, DeviceInfo info, EngineState state) {
        this(c, pins, features, info, state, new Backoff(BACKOFF_MS, new java.util.Random()), UPGRADE_CHECK_MS);
    }

    /** Tests inject the backoff and a short USB-upgrade interval. */
    public LinkEngine(Connector c, PinStore pins, FeatureRegistry features, DeviceInfo info, EngineState state,
                      Backoff backoff, long upgradeCheckMs) {
        this.backoff = backoff;
        this.upgradeCheckMs = upgradeCheckMs;
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
                state.setTransport(connector.transport());
                Session s = new Session(link, pins, features, info, state);
                session = s;
                long t0 = System.currentTimeMillis();
                Thread upgrade = watchForUsb(s);
                try {
                    s.run();
                } finally {
                    if (upgrade != null) upgrade.interrupt();
                    connector.disconnected();
                    state.setTransport("none");
                }
                session = null;
                failures = System.currentTimeMillis() - t0 > 20000 ? 0 : failures + 1;
            } catch (IOException e) {
                session = null;
                state.setTransport("none");
                if (state.phase() != EngineState.Phase.ERROR) state.set(EngineState.Phase.DISCONNECTED, "", "", "No desktop reachable");
                failures++;
            } catch (RuntimeException e) {
                session = null;
                state.set(EngineState.Phase.ERROR, "", "", "Internal error: " + e.getClass().getSimpleName());
                failures++;
            }
            try {
                Thread.sleep(backoff.delayMs(failures));
            } catch (InterruptedException e) {
                return;
            }
        }
    }

    /**
     * While on LAN, checks periodically whether USB answers and then ends the session so the next
     * connect() takes USB (a cable beats Wi-Fi). Null when the link is already USB.
     */
    private Thread watchForUsb(Session s) {
        if (!"lan".equals(state.transport())) return null;
        Thread t = new Thread(() -> {
            try {
                while (running) {
                    Thread.sleep(upgradeCheckMs);
                    if (connector.upgradeAvailable()) {
                        s.command(Session.Command.STOP);
                        return;
                    }
                }
            } catch (InterruptedException ignored) { /* session ended */ }
        }, "qindalink-usb-watch");
        t.setDaemon(true);
        t.start();
        return t;
    }
}
