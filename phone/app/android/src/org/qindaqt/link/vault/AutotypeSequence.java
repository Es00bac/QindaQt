// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Parser for the per-entry autotype sequence.
 *
 * <pre>
 * sequence := ( literal | token )*
 * literal  := any character except '{' and '}'          (typed as text, spaces included)
 * token    := '{USERNAME}' | '{PASSWORD}' | '{URL}'     (field values)
 *           | '{TAB}' | '{ENTER}'                        (shorthand for KEY)
 *           | '{KEY name}'                               (name in {@link #KEYS})
 *           | '{DELAY ms}'                               (0..10000 ms, total at most 30000)
 *           | '{{}' | '{}}'                              (a literal '{' or '}')
 * </pre>
 *
 * The default is {@code {USERNAME}{TAB}{PASSWORD}{ENTER}}. Limits: at most 256 characters and 64
 * items. A lone '}' or '{', an unknown name, a bad number or an unterminated token is rejected with
 * the offending character position (never the content). Names are upper case and case sensitive.
 * AGENT-CONTRACT: part 2 (vault.type) maps TEXT/KEY/DELAY steps onto its frame; keep the key names
 * stable, they are the wire vocabulary.
 */
public final class AutotypeSequence {
    public static final int MAX_LENGTH = 256, MAX_ITEMS = 64, MAX_DELAY_MS = 10_000, MAX_TOTAL_DELAY_MS = 30_000;
    public static final Set<String> KEYS = Collections.unmodifiableSet(new HashSet<>(Arrays.asList(
        "TAB", "ENTER", "ESC", "BACKSPACE", "DELETE", "SPACE", "UP", "DOWN", "LEFT", "RIGHT", "HOME", "END")));

    /** What one parsed item is. */
    public enum Kind { LITERAL, USERNAME, PASSWORD, URL, KEY, DELAY }

    /** One parsed item: {@code text} for LITERAL and KEY (the key name), {@code ms} for DELAY. */
    public static final class Item {
        public final Kind kind;
        public final String text;
        public final int ms;
        Item(Kind kind, String text, int ms) { this.kind = kind; this.text = text; this.ms = ms; }
    }

    /** Thrown for malformed input; {@link #position()} is a 0-based character offset. */
    public static final class SyntaxException extends Exception {
        private static final long serialVersionUID = 1L;
        private final int position;
        SyntaxException(String what, int position) { super(what + " at " + position); this.position = position; }
        public int position() { return position; }
    }

    private final List<Item> items;
    private AutotypeSequence(List<Item> items) { this.items = Collections.unmodifiableList(items); }

    public List<Item> items() { return items; }

    public static AutotypeSequence parse(String s) throws SyntaxException {
        if (s.length() > MAX_LENGTH) throw new SyntaxException("sequence too long", MAX_LENGTH);
        List<Item> out = new ArrayList<>();
        StringBuilder lit = new StringBuilder();
        int totalDelay = 0;
        for (int i = 0; i < s.length(); ) {
            char c = s.charAt(i);
            if (c == '}') throw new SyntaxException("unexpected '}'", i);
            if (c != '{') { lit.append(c); i++; continue; }
            int close = s.indexOf('}', i + 1);
            // "{}}" and "{{}" are the escapes; the latter closes at the first '}' after the second '{'.
            if (s.startsWith("{{}", i)) { lit.append('{'); i += 3; continue; }
            if (s.startsWith("{}}", i)) { lit.append('}'); i += 3; continue; }
            if (close < 0) throw new SyntaxException("unterminated token", i);
            String body = s.substring(i + 1, close);
            if (lit.length() > 0) { out.add(new Item(Kind.LITERAL, lit.toString(), 0)); lit.setLength(0); }
            String[] p = body.split(" ", -1);
            switch (p[0]) {
                case "USERNAME": noArg(p, i); out.add(new Item(Kind.USERNAME, null, 0)); break;
                case "PASSWORD": noArg(p, i); out.add(new Item(Kind.PASSWORD, null, 0)); break;
                case "URL": noArg(p, i); out.add(new Item(Kind.URL, null, 0)); break;
                case "TAB": case "ENTER": noArg(p, i); out.add(new Item(Kind.KEY, p[0], 0)); break;
                case "KEY":
                    if (p.length != 2 || !KEYS.contains(p[1])) throw new SyntaxException("unknown key", i);
                    out.add(new Item(Kind.KEY, p[1], 0));
                    break;
                case "DELAY": {
                    if (p.length != 2 || p[1].isEmpty() || p[1].length() > 5 || !p[1].chars().allMatch(ch -> ch >= '0' && ch <= '9'))
                        throw new SyntaxException("bad delay", i);
                    int ms = Integer.parseInt(p[1]);
                    totalDelay += ms;
                    if (ms > MAX_DELAY_MS || totalDelay > MAX_TOTAL_DELAY_MS) throw new SyntaxException("delay too long", i);
                    out.add(new Item(Kind.DELAY, null, ms));
                    break;
                }
                default: throw new SyntaxException("unknown token", i);
            }
            i = close + 1;
        }
        if (lit.length() > 0) out.add(new Item(Kind.LITERAL, lit.toString(), 0));
        if (out.size() > MAX_ITEMS) throw new SyntaxException("too many items", 0);
        return new AutotypeSequence(out);
    }

    private static void noArg(String[] p, int pos) throws SyntaxException {
        if (p.length != 1) throw new SyntaxException("unexpected argument", pos);
    }
}
