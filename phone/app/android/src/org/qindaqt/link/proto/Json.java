package org.qindaqt.link.proto;

import java.math.BigInteger;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Strict RFC 8259 JSON reader/writer with no android.* or org.json dependency.
 *
 * AGENT-NOTE: this exists instead of org.json so the JVM tests and the APK run
 * byte-for-byte the same parser (org.json on Android and on Maven differ on
 * fractions, big integers and trailing garbage). Value mapping: object ->
 * LinkedHashMap, array -> ArrayList, string -> String, integer -> Long (or
 * BigInteger when it overflows long), fraction/exponent -> Double, true/false ->
 * Boolean, null -> {@link #NULL}. Duplicate object keys: the last one wins.
 */
public final class Json {
    /** Marker for JSON null; kept distinct from "key absent". */
    public static final Object NULL = new Object() {
        @Override public String toString() { return "null"; }
    };

    /** Thrown for any malformed document. */
    public static final class ParseException extends Exception {
        private static final long serialVersionUID = 1L;
        ParseException(String m) { super(m); }
    }

    /** Protocol limit: the top-level object is level 1; level 65 is rejected. */
    public static final int PROTOCOL_MAX_LEVELS = 64;
    private final String s;
    private final int maxLevels;
    private int i;

    private Json(String s, int maxLevels) { this.s = s; this.maxLevels = maxLevels; }

    public static Object parse(String text) throws ParseException { return parse(text, PROTOCOL_MAX_LEVELS); }

    /** Test fixtures that embed protocol payloads need a deeper limit than the wire does. */
    public static Object parse(String text, int maxLevels) throws ParseException {
        Json p = new Json(text, maxLevels);
        p.ws();
        Object v = p.value(0);
        p.ws();
        if (p.i != p.s.length()) throw new ParseException("trailing data");
        return v;
    }

    private void ws() {
        while (i < s.length()) {
            char c = s.charAt(i);
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') i++; else break;
        }
    }

    private Object value(int depth) throws ParseException {
        if (i >= s.length()) throw new ParseException("eof");
        char c = s.charAt(i);
        switch (c) {
            case '{': return object(depth);
            case '[': return array(depth);
            case '"': return string();
            case 't': lit("true"); return Boolean.TRUE;
            case 'f': lit("false"); return Boolean.FALSE;
            case 'n': lit("null"); return NULL;
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return number();
                throw new ParseException("unexpected character");
        }
    }

    private void lit(String w) throws ParseException {
        if (!s.startsWith(w, i)) throw new ParseException("bad literal");
        i += w.length();
    }

    private Map<String, Object> object(int depth) throws ParseException {
        if (depth >= maxLevels) throw new ParseException("too deep");
        Map<String, Object> m = new LinkedHashMap<>();
        i++;
        ws();
        if (peek() == '}') { i++; return m; }
        while (true) {
            ws();
            if (peek() != '"') throw new ParseException("key expected");
            String k = string();
            ws();
            if (peek() != ':') throw new ParseException("colon expected");
            i++;
            ws();
            m.put(k, value(depth + 1));
            ws();
            char c = peek();
            i++;
            if (c == '}') return m;
            if (c != ',') throw new ParseException("comma expected");
        }
    }

    private List<Object> array(int depth) throws ParseException {
        if (depth >= maxLevels) throw new ParseException("too deep");
        List<Object> l = new ArrayList<>();
        i++;
        ws();
        if (peek() == ']') { i++; return l; }
        while (true) {
            ws();
            l.add(value(depth + 1));
            ws();
            char c = peek();
            i++;
            if (c == ']') return l;
            if (c != ',') throw new ParseException("comma expected");
        }
    }

    private char peek() throws ParseException {
        if (i >= s.length()) throw new ParseException("eof");
        return s.charAt(i);
    }

    private String string() throws ParseException {
        StringBuilder b = new StringBuilder();
        i++;
        while (true) {
            if (i >= s.length()) throw new ParseException("unterminated string");
            char c = s.charAt(i++);
            if (c == '"') return b.toString();
            if (c < 0x20) throw new ParseException("control character in string");
            if (c != '\\') { b.append(c); continue; }
            if (i >= s.length()) throw new ParseException("bad escape");
            char e = s.charAt(i++);
            switch (e) {
                case '"': b.append('"'); break;
                case '\\': b.append('\\'); break;
                case '/': b.append('/'); break;
                case 'b': b.append('\b'); break;
                case 'f': b.append('\f'); break;
                case 'n': b.append('\n'); break;
                case 'r': b.append('\r'); break;
                case 't': b.append('\t'); break;
                case 'u':
                    if (i + 4 > s.length()) throw new ParseException("bad unicode escape");
                    int cp = 0;
                    for (int k = 0; k < 4; k++) {
                        int d = Character.digit(s.charAt(i + k), 16);
                        if (d < 0) throw new ParseException("bad unicode escape");
                        cp = cp * 16 + d;
                    }
                    i += 4;
                    b.append((char) cp);
                    break;
                default: throw new ParseException("bad escape");
            }
        }
    }

    private Object number() throws ParseException {
        int st = i;
        if (peek() == '-') i++;
        if (i >= s.length()) throw new ParseException("bad number");
        if (s.charAt(i) == '0') {
            i++;
        } else if (s.charAt(i) >= '1' && s.charAt(i) <= '9') {
            while (i < s.length() && isDigit(s.charAt(i))) i++;
        } else {
            throw new ParseException("bad number");
        }
        boolean frac = false;
        if (i < s.length() && s.charAt(i) == '.') {
            frac = true;
            i++;
            int d = i;
            while (i < s.length() && isDigit(s.charAt(i))) i++;
            if (i == d) throw new ParseException("bad number");
        }
        if (i < s.length() && (s.charAt(i) == 'e' || s.charAt(i) == 'E')) {
            frac = true;
            i++;
            if (i < s.length() && (s.charAt(i) == '+' || s.charAt(i) == '-')) i++;
            int d = i;
            while (i < s.length() && isDigit(s.charAt(i))) i++;
            if (i == d) throw new ParseException("bad number");
        }
        String t = s.substring(st, i);
        if (frac) return Double.valueOf(t);
        BigInteger bi = new BigInteger(t);
        return bi.bitLength() < 64 ? (Object) Long.valueOf(bi.longValue()) : bi;
    }

    private static boolean isDigit(char c) { return c >= '0' && c <= '9'; }

    /** Compact serialisation. Supports the types {@link #parse} produces. */
    public static String write(Object v) {
        StringBuilder b = new StringBuilder();
        write(b, v);
        return b.toString();
    }

    @SuppressWarnings("unchecked")
    private static void write(StringBuilder b, Object v) {
        if (v == null || v == NULL) {
            b.append("null");
        } else if (v instanceof Map) {
            b.append('{');
            boolean first = true;
            for (Map.Entry<String, Object> e : ((Map<String, Object>) v).entrySet()) {
                if (!first) b.append(',');
                first = false;
                str(b, e.getKey());
                b.append(':');
                write(b, e.getValue());
            }
            b.append('}');
        } else if (v instanceof List) {
            b.append('[');
            boolean first = true;
            for (Object o : (List<Object>) v) {
                if (!first) b.append(',');
                first = false;
                write(b, o);
            }
            b.append(']');
        } else if (v instanceof String) {
            str(b, (String) v);
        } else if (v instanceof Boolean || v instanceof Long || v instanceof Integer
                || v instanceof BigInteger) {
            b.append(v.toString());
        } else if (v instanceof Double) {
            double d = (Double) v;
            if (Double.isNaN(d) || Double.isInfinite(d)) throw new IllegalArgumentException("non-finite");
            b.append(v.toString());
        } else {
            throw new IllegalArgumentException("unsupported type " + v.getClass());
        }
    }

    private static void str(StringBuilder b, String s) {
        b.append('"');
        for (int k = 0; k < s.length(); k++) {
            char c = s.charAt(k);
            switch (c) {
                case '"': b.append("\\\""); break;
                case '\\': b.append("\\\\"); break;
                case '\n': b.append("\\n"); break;
                case '\r': b.append("\\r"); break;
                case '\t': b.append("\\t"); break;
                case '\b': b.append("\\b"); break;
                case '\f': b.append("\\f"); break;
                default:
                    if (c < 0x20) b.append(String.format(java.util.Locale.ROOT, "\\u%04x", (int) c)); else b.append(c);
            }
        }
        b.append('"');
    }
}
