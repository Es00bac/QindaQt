package org.qindaqt.link.input;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import org.qindaqt.link.proto.Envelope;

/**
 * Builders for the phone-to-desktop {@code input.*} frames, with the protocol-page bounds applied
 * (docs/wiki/reference/qindalink-protocol-v1.md, "Remote input"). Pure Java, no android.*.
 *
 * AGENT-CONTRACT: the desktop side is src/services/link_input input_frames.cpp. A builder never
 * produces a frame the desktop would reject: out-of-range numbers are clamped, non-finite values
 * become 0, and unknown names return null.
 */
public final class InputFrames {
    private InputFrames() {}

    public static final double MAX_POINTER = 1000.0;
    public static final double MAX_SMOOTH_SCROLL = 500.0;
    public static final int MAX_DISCRETE = 20;
    public static final int MAX_TEXT_BYTES = 4096;

    /** The closed key-name set of the protocol page. */
    public static final Set<String> KEY_NAMES = new HashSet<>(Arrays.asList(
            "Escape", "Tab", "Return", "BackSpace", "Delete", "Insert", "Home", "End", "PageUp", "PageDown",
            "Left", "Right", "Up", "Down", "Space", "Menu", "Print", "CapsLock", "Shift", "Ctrl", "Alt", "Meta",
            "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12"));

    private static final Set<String> BUTTONS = new HashSet<>(Arrays.asList("left", "right", "middle"));
    private static final Set<String> BUTTON_STATES = new HashSet<>(Arrays.asList("down", "up", "click"));
    private static final Set<String> KEY_STATES = new HashSet<>(Arrays.asList("down", "up", "tap"));

    static double clamp(double v, double limit) {
        if (Double.isNaN(v) || Double.isInfinite(v)) return 0;
        return Math.max(-limit, Math.min(limit, v));
    }

    /** Rounded to 0.01 so frames stay short; clamped to the protocol bound. */
    static double round(double v) { return Math.rint(v * 100.0) / 100.0; }

    public static Envelope pointer(double dx, double dy) {
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("dx", round(clamp(dx, MAX_POINTER)));
        b.put("dy", round(clamp(dy, MAX_POINTER)));
        return Envelope.of("input.pointer", b);
    }

    /** {@code button}: left, right or middle; {@code state}: down, up or click. Null if unknown. */
    public static Envelope button(String button, String state) {
        if (!BUTTONS.contains(button) || !BUTTON_STATES.contains(state)) return null;
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("button", button);
        b.put("state", state);
        return Envelope.of("input.button", b);
    }

    /** Smooth scroll; positive dy moves the content toward its end. */
    public static Envelope scroll(double dx, double dy, boolean end) {
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("mode", "smooth");
        b.put("dx", round(clamp(dx, MAX_SMOOTH_SCROLL)));
        b.put("dy", round(clamp(dy, MAX_SMOOTH_SCROLL)));
        if (end) b.put("end", Boolean.TRUE);
        return Envelope.of("input.scroll", b);
    }

    public static Envelope scrollDiscrete(int dx, int dy) {
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("mode", "discrete");
        b.put("dx", (long) Math.max(-MAX_DISCRETE, Math.min(MAX_DISCRETE, dx)));
        b.put("dy", (long) Math.max(-MAX_DISCRETE, Math.min(MAX_DISCRETE, dy)));
        return Envelope.of("input.scroll", b);
    }

    /** A named key (see {@link #KEY_NAMES}); null if the name or state is unknown. */
    public static Envelope key(String name, String state) {
        if (!KEY_NAMES.contains(name) || !KEY_STATES.contains(state)) return null;
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("name", name);
        b.put("state", state);
        return Envelope.of("input.key", b);
    }

    /** An X11 keysym key; null if the keysym is outside the allowed ranges. */
    public static Envelope keysym(long keysym, String state) {
        if (!validKeysym(keysym) || !KEY_STATES.contains(state)) return null;
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("keysym", keysym);
        b.put("state", state);
        return Envelope.of("input.key", b);
    }

    public static boolean validKeysym(long k) {
        return (k >= 0x20 && k <= 0x7e) || (k >= 0xa0 && k <= 0xff) || (k >= 0xfe00 && k <= 0xffff)
                || (k >= 0x1008ff00L && k <= 0x1008ffffL)
                || (k >= 0x01000100L && k <= 0x0110ffffL && !(k >= 0x0100d800L && k <= 0x0100dfffL));
    }

    /** One text frame, or null when the text is empty or over 4096 UTF-8 bytes (use {@link #splitText}). */
    public static Envelope text(String text) {
        if (text == null || text.isEmpty() || text.getBytes(StandardCharsets.UTF_8).length > MAX_TEXT_BYTES) return null;
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("text", text);
        return Envelope.of("input.text", b);
    }

    /**
     * Splits text into chunks of at most {@code maxBytes} UTF-8 bytes without cutting a code point.
     * The sender paces the chunks because the desktop refuses a second text frame while one is typing.
     */
    public static List<String> splitText(String text, int maxBytes) {
        List<String> out = new ArrayList<>();
        if (text == null) return out;
        int limit = Math.max(4, Math.min(maxBytes, MAX_TEXT_BYTES));
        StringBuilder cur = new StringBuilder();
        int bytes = 0;
        for (int i = 0; i < text.length(); ) {
            int cp = text.codePointAt(i);
            int n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
            if (bytes + n > limit) {
                out.add(cur.toString());
                cur.setLength(0);
                bytes = 0;
            }
            cur.appendCodePoint(cp);
            bytes += n;
            i += Character.charCount(cp);
        }
        if (cur.length() > 0) out.add(cur.toString());
        return out;
    }
}
