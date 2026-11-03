package org.qindaqt.link.engine;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Thread-safe set of desktops currently visible, keyed by mDNS instance name. Android-free. */
public final class DiscoveryTable {
    private final Map<String, DiscoveredDesktop> byKey = new LinkedHashMap<>();
    private boolean active;

    public synchronized void found(String key, DiscoveredDesktop d) {
        if (key != null && d != null) byKey.put(key, d);
    }

    public synchronized void lost(String key) { byKey.remove(key); }

    public synchronized void clear() { byKey.clear(); }

    public synchronized void setActive(boolean on) { active = on; }

    public synchronized boolean active() { return active; }

    /** Sorted by name then id so planning is deterministic. */
    public synchronized List<DiscoveredDesktop> snapshot() {
        List<DiscoveredDesktop> out = new ArrayList<>(byKey.values());
        out.sort(Comparator.comparing((DiscoveredDesktop d) -> d.name).thenComparing(d -> d.id));
        return out;
    }
}
