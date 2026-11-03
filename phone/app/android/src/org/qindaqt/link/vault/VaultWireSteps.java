// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Turns {@link TypingStep}s into the {@code steps} array of a {@code vault.type} frame (protocol page,
 * "Desktop facts and secret typing"; ADR-0440). Pure Java.
 *
 * Limits mirror the desktop: at most 16 steps, text at most 1024 UTF-8 bytes per step (longer text is
 * split on character boundaries), delays at most 2000 ms (longer ones are split), and a closed key
 * set Tab/Enter/Escape/arrows. The autotype keys SPACE becomes a space of text; BACKSPACE, DELETE,
 * HOME and END have no wire form and make the whole request fail before anything is sent.
 * Control characters inside text are left as text: the desktop skips them, they never become keys.
 *
 * AGENT-GUARD: {@link #validate} touches no text, so the UI can refuse early without creating Strings;
 * {@link #encode} creates one String per text chunk (the codec needs it), so call it as late as
 * possible, on the session thread, and drop the result after sending. Never log either result.
 */
public final class VaultWireSteps {
    public static final int MAX_STEPS = 16, MAX_TEXT_BYTES = 1024, MAX_DELAY_MS = 2000;

    private VaultWireSteps() {}

    /** Why a plan cannot be sent; the message is safe to show (it never contains step text). */
    public static final class Unsupported extends Exception {
        Unsupported(String message) { super(message); }
    }

    private static String wireKey(String autotypeKey) {
        switch (autotypeKey) {
            case "TAB": return "Tab";
            case "ENTER": return "Enter";
            case "ESC": return "Escape";
            case "UP": return "Up";
            case "DOWN": return "Down";
            case "LEFT": return "Left";
            case "RIGHT": return "Right";
            default: return null;
        }
    }

    private static int chunks(int bytes) { return bytes == 0 ? 0 : (bytes + MAX_TEXT_BYTES - 1) / MAX_TEXT_BYTES; }

    /** Counts and checks without creating any text. */
    public static void validate(List<TypingStep> steps) throws Unsupported {
        int count = 0;
        boolean types = false;
        for (TypingStep s : steps) {
            switch (s.type) {
                case TEXT: {
                    int n = chunks(s.textBytes().length);   // an upper bound: a split on a boundary can add one more
                    count += n;
                    types |= n > 0;
                    break;
                }
                case KEY:
                    if (s.key.equals("SPACE")) { count++; types = true; break; }
                    if (wireKey(s.key) == null) throw new Unsupported("The autotype key " + s.key + " cannot be typed on the desktop.");
                    count++;
                    types = true;
                    break;
                default:
                    count += Math.max(1, (s.delayMs + MAX_DELAY_MS - 1) / MAX_DELAY_MS);
            }
        }
        if (!types) throw new Unsupported("There is nothing to type.");
        if (count > MAX_STEPS) throw new Unsupported("This sequence is too long to send in one go.");
    }

    /** Builds the wire steps; call {@link #validate} first. Throws if the exact count exceeds the limit. */
    public static List<Map<String, Object>> encode(List<TypingStep> steps) throws Unsupported {
        List<Map<String, Object>> out = new ArrayList<>();
        for (TypingStep s : steps) {
            switch (s.type) {
                case TEXT: {
                    byte[] b = s.textBytes();
                    int from = 0;
                    while (from < b.length) {
                        int to = Math.min(b.length, from + MAX_TEXT_BYTES);
                        while (to < b.length && to > from && (b[to] & 0xC0) == 0x80) to--;   // never split a character
                        if (to == from) throw new Unsupported("A character is too large to send.");
                        out.add(one("text", new String(b, from, to - from, StandardCharsets.UTF_8)));
                        from = to;
                    }
                    break;
                }
                case KEY:
                    if (s.key.equals("SPACE")) out.add(one("text", " "));
                    else out.add(one("key", wireKey(s.key)));
                    break;
                default: {
                    int left = Math.max(0, s.delayMs);
                    do {
                        int d = Math.min(left, MAX_DELAY_MS);
                        out.add(one("delayMs", (long) d));
                        left -= d;
                    } while (left > 0);
                }
            }
        }
        if (out.size() > MAX_STEPS) throw new Unsupported("This sequence is too long to send in one go.");
        return out;
    }

    private static Map<String, Object> one(String k, Object v) {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put(k, v);
        return m;
    }
}
