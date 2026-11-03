// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.nio.charset.StandardCharsets;

/**
 * One typing step for the desktop: text (UTF-8 bytes, zeroed on close), a named key, or a delay.
 * AGENT-CONTRACT: the vocabulary (text/key/delay) is what part 2's vault.type frame carries.
 */
public final class TypingStep {
    public enum Type { TEXT, KEY, DELAY }

    public final Type type;
    private final byte[] text;
    public final String key;
    public final int delayMs;

    private TypingStep(Type type, byte[] text, String key, int delayMs) { this.type = type; this.text = text; this.key = key; this.delayMs = delayMs; }

    static TypingStep text(byte[] utf8) { return new TypingStep(Type.TEXT, utf8, null, 0); }
    static TypingStep key(String name) { return new TypingStep(Type.KEY, null, name, 0); }
    static TypingStep delay(int ms) { return new TypingStep(Type.DELAY, null, null, ms); }

    /** The live text buffer (not a copy); null for non-text steps. Valid until the source closes. */
    public byte[] textBytes() { return text; }

    /** Convenience for tests; creates an unzeroable String, so production code uses {@link #textBytes()}. */
    public String textString() { return text == null ? null : new String(text, StandardCharsets.UTF_8); }

    void wipe() { Bin.wipe(text); }

    @Override public String toString() { return "TypingStep[" + type + "]"; }
}
