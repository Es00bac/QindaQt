package org.qindaqt.link.engine;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.TreeSet;

/**
 * Handlers injected into a {@link LinkEngine} (by LinkService on Android, by tests
 * elsewhere). Mirror of QindaQt::Link::FeatureRouter. Build it fully before the
 * first session starts; it is read-only afterwards.
 */
public final class FeatureRegistry {
    private final Map<String, FeatureHandler> byNamespace = new LinkedHashMap<>();
    private final List<FeatureHandler> all = new ArrayList<>();

    /** Registers a handler; a namespace may belong to only one handler. */
    public FeatureRegistry add(FeatureHandler h) {
        for (String ns : h.namespaces()) {
            if (byNamespace.put(ns, h) != null) throw new IllegalArgumentException("namespace taken: " + ns);
        }
        all.add(h);
        return this;
    }

    /** Handler owning the namespace of a frame type, or null. */
    public FeatureHandler find(String frameType) {
        int dot = frameType.indexOf('.');
        return byNamespace.get(dot < 0 ? frameType : frameType.substring(0, dot));
    }

    public List<FeatureHandler> handlers() { return Collections.unmodifiableList(all); }

    /** Sorted capability strings for our hello. */
    public List<String> caps() {
        TreeSet<String> s = new TreeSet<>();
        for (FeatureHandler h : all) s.add(h.capability());
        return new ArrayList<>(s);
    }
}
