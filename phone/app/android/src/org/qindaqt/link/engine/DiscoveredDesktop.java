package org.qindaqt.link.engine;

import java.util.Map;

/**
 * A desktop seen through mDNS (_qindalink._tcp). Built only from a TXT record that passes
 * {@link #fromTxt}; nothing in it is trusted: the pin is verified in TLS regardless.
 */
public final class DiscoveredDesktop {
    public final String id;    // first 16 hex characters of the desktop SPKI digest
    public final String name;
    public final String host;
    public final int port;

    DiscoveredDesktop(String id, String name, String host, int port) {
        this.id = id;
        this.name = name;
        this.host = host;
        this.port = port;
    }

    /** Null unless proto is "1" and id is exactly 16 lowercase hex characters. */
    public static DiscoveredDesktop fromTxt(String instanceName, String host, int port, Map<String, String> txt) {
        if (host == null || host.isEmpty() || port < 1 || port > 65535 || txt == null) return null;
        if (!"1".equals(txt.get("proto"))) return null;
        String id = txt.get("id");
        if (id == null || !id.matches("[0-9a-f]{16}")) return null;
        String name = txt.get("name");
        if (name == null || name.trim().isEmpty()) name = instanceName == null ? "desktop" : instanceName;
        name = name.replaceAll("\\p{Cntrl}", "").trim();
        if (name.length() > 64) name = name.substring(0, 64);
        return new DiscoveredDesktop(id, name.isEmpty() ? "desktop" : name, host, port);
    }

    public Endpoint endpoint() { return new Endpoint(host, port, Endpoint.Kind.LAN_DISCOVERED); }
}
