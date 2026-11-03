package org.qindaqt.link.engine;

import java.util.Locale;

/**
 * Where to reach a desktop: a host and port plus where the address came from.
 * Android-free. Equality ignores the kind so the same address is never tried twice.
 *
 * AGENT-CONTRACT: {@link #transport()} is the truthful transport reported in EngineState
 * ("usb" only for the adb-reversed loopback address, "lan" for every network address, Wi-Fi or Tailscale).
 */
public final class Endpoint {
    /** Order here is the order the planner tries them. */
    public enum Kind { USB, LAN_LAST, LAN_DISCOVERED, LAN_MANUAL }

    public static final int DEFAULT_PORT = 47160;

    public final String host;
    public final int port;
    public final Kind kind;

    public Endpoint(String host, int port, Kind kind) {
        this.host = host;
        this.port = port;
        this.kind = kind;
    }

    /** The adb-reversed loopback listener (ADR-0434). */
    public static Endpoint usb() { return new Endpoint("127.0.0.1", DEFAULT_PORT, Kind.USB); }

    public String transport() { return kind == Kind.USB ? "usb" : "lan"; }

    /** host:port, with brackets around an IPv6 literal. Parseable by {@link ManualAddress}. */
    public String hostPort() { return (host.indexOf(':') >= 0 ? "[" + host + "]" : host) + ":" + port; }

    @Override public boolean equals(Object o) {
        if (!(o instanceof Endpoint)) return false;
        Endpoint e = (Endpoint) o;
        return port == e.port && host.equalsIgnoreCase(e.host);
    }

    @Override public int hashCode() { return host.toLowerCase(Locale.ROOT).hashCode() * 31 + port; }

    /** Never logged with the host: addresses are personal. */
    @Override public String toString() { return kind + "@:" + port; }
}
