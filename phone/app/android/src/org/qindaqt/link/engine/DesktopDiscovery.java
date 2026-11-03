package org.qindaqt.link.engine;

/**
 * Finds desktops on the local network and reports them to a {@link DiscoveryTable}. Android's
 * NsdManager implements it; tests use a fake.
 *
 * AGENT-CONTRACT: {@link #start} and {@link #stop} are idempotent and cheap to call from the engine
 * thread; callbacks into the table may come from any thread. Discovery must be stopped whenever a
 * session is up (battery).
 */
public interface DesktopDiscovery {
    void start(DiscoveryTable sink);
    void stop();
}
