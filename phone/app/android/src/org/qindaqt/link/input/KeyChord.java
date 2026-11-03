package org.qindaqt.link.input;

import java.util.ArrayList;
import java.util.List;
import org.qindaqt.link.proto.Envelope;

/**
 * Expands a latched-modifier chord into the ordered {@code input.key} frames of the protocol page:
 * modifiers down (Ctrl, Alt, Shift, Meta), the key, then modifiers up in reverse. Pure Java.
 */
public final class KeyChord {
    private KeyChord() {}

    public static final int CTRL = 1, ALT = 2, SHIFT = 4, META = 8;
    private static final String[] NAMES = {"Ctrl", "Alt", "Shift", "Meta"};
    private static final int[] BITS = {CTRL, ALT, SHIFT, META};

    /** Frames for {@code name} pressed with the modifier bit mask; an unknown name yields an empty list. */
    public static List<Envelope> frames(String name, int mods) {
        List<Envelope> out = new ArrayList<>();
        if (!InputFrames.KEY_NAMES.contains(name)) return out;
        for (int i = 0; i < NAMES.length; i++) if ((mods & BITS[i]) != 0) out.add(InputFrames.key(NAMES[i], "down"));
        out.add(InputFrames.key(name, "tap"));
        for (int i = NAMES.length - 1; i >= 0; i--) if ((mods & BITS[i]) != 0) out.add(InputFrames.key(NAMES[i], "up"));
        return out;
    }

    /** Same for a printable character; Unicode above U+00FF uses 0x01000000 | codepoint. */
    public static List<Envelope> frames(int codePoint, int mods) {
        List<Envelope> out = new ArrayList<>();
        long keysym = codePoint <= 0xff ? codePoint : (0x01000000L | codePoint);
        if (!InputFrames.validKeysym(keysym)) return out;
        for (int i = 0; i < NAMES.length; i++) if ((mods & BITS[i]) != 0) out.add(InputFrames.key(NAMES[i], "down"));
        out.add(InputFrames.keysym(keysym, "tap"));
        for (int i = NAMES.length - 1; i >= 0; i--) if ((mods & BITS[i]) != 0) out.add(InputFrames.key(NAMES[i], "up"));
        return out;
    }
}
