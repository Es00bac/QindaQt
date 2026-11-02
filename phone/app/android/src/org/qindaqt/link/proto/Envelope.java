package org.qindaqt.link.proto;

import java.util.LinkedHashMap;
import java.util.Map;
import java.util.regex.Pattern;

/**
 * One decoded protocol frame: type {@code t}, optional ids, body {@code b}.
 * Immutable-by-convention value type; the body map must not be mutated after
 * construction.
 */
public final class Envelope {
    public static final long MAX_ID = 9007199254740991L;
    private static final Pattern TYPE = Pattern.compile("[a-z][a-z0-9]*(\\.[a-z][a-z0-9-]*)*");

    public final String t;
    /** 0 when absent (valid ids start at 1). */
    public final long id;
    /** 0 when absent. */
    public final long re;
    public final Map<String, Object> b;

    public Envelope(String t, long id, long re, Map<String, Object> b) {
        this.t = t;
        this.id = id;
        this.re = re;
        this.b = b == null ? new LinkedHashMap<String, Object>() : b;
    }

    public static Envelope of(String t, Map<String, Object> b) { return new Envelope(t, 0, 0, b); }

    /** Validates envelope rules from the protocol page; null means bad-envelope. */
    @SuppressWarnings("unchecked")
    static Envelope fromObject(Map<String, Object> o) {
        Object t = o.get("t");
        if (!(t instanceof String)) return null;
        String ts = (String) t;
        if (ts.length() < 1 || ts.length() > 64 || !TYPE.matcher(ts).matches()) return null;
        long id = 0, re = 0;
        if (o.containsKey("id")) { id = idOf(o.get("id")); if (id < 0) return null; }
        if (o.containsKey("re")) { re = idOf(o.get("re")); if (re < 0) return null; }
        Map<String, Object> b = null;
        if (o.containsKey("b")) {
            if (!(o.get("b") instanceof Map)) return null;
            b = (Map<String, Object>) o.get("b");
        }
        return new Envelope(ts, id, re, b);
    }

    private static long idOf(Object v) {
        if (!(v instanceof Long)) return -1;
        long l = (Long) v;
        return (l >= 1 && l <= MAX_ID) ? l : -1;
    }

    /** Map form used by the encoder and the vector tests. */
    public Map<String, Object> toObject() {
        Map<String, Object> o = new LinkedHashMap<>();
        o.put("t", t);
        if (id != 0) o.put("id", id);
        if (re != 0) o.put("re", re);
        if (!b.isEmpty()) o.put("b", b);
        return o;
    }
}
