// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.security.SecureRandom;
import java.util.Random;

/** Random passwords and word passphrases. Uniform selection (Random.nextInt is unbiased). */
public final class PasswordGenerator {
    public static final int MIN_LENGTH = 8, MAX_LENGTH = 128, MIN_WORDS = 3, MAX_WORDS = 12;
    private static final String LOWER = "abcdefghijklmnopqrstuvwxyz", UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
        DIGITS = "0123456789", SYMBOLS = "!@#$%^&*()-_=+[]{};:,.?/~", AMBIGUOUS = "0O1lI|";

    private final Random rng;

    /** Production generator on {@link SecureRandom}. */
    public PasswordGenerator() { this(new SecureRandom()); }

    /** Tests pass a seeded Random. */
    public PasswordGenerator(Random rng) { this.rng = rng; }

    /**
     * A password of exactly {@code length} characters containing at least one character of every
     * selected class. Caller zeroes the array.
     */
    public char[] password(int length, boolean lower, boolean upper, boolean digits, boolean symbols, boolean avoidAmbiguous) {
        if (length < MIN_LENGTH || length > MAX_LENGTH) throw new IllegalArgumentException("length out of range");
        java.util.List<String> classes = new java.util.ArrayList<>();
        if (lower) classes.add(filter(LOWER, avoidAmbiguous));
        if (upper) classes.add(filter(UPPER, avoidAmbiguous));
        if (digits) classes.add(filter(DIGITS, avoidAmbiguous));
        if (symbols) classes.add(filter(SYMBOLS, avoidAmbiguous));
        if (classes.isEmpty()) throw new IllegalArgumentException("no character class selected");
        StringBuilder all = new StringBuilder();
        for (String c : classes) all.append(c);
        char[] out = new char[length];
        int i = 0;
        for (String c : classes) out[i++] = c.charAt(rng.nextInt(c.length()));
        while (i < length) out[i++] = all.charAt(rng.nextInt(all.length()));
        for (int k = length - 1; k > 0; k--) {   // Fisher-Yates so the guaranteed characters land anywhere
            int j = rng.nextInt(k + 1);
            char t = out[k]; out[k] = out[j]; out[j] = t;
        }
        return out;
    }

    /** {@code words} words from the EFF large list joined by {@code separator}; optionally capitalised plus a trailing digit. */
    public char[] passphrase(int words, String separator, boolean capitalize, boolean addNumber) {
        if (words < MIN_WORDS || words > MAX_WORDS) throw new IllegalArgumentException("word count out of range");
        String[] list = WordList.words();
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < words; i++) {
            String w = list[rng.nextInt(list.length)];
            if (i > 0) sb.append(separator);
            sb.append(capitalize ? Character.toUpperCase(w.charAt(0)) + w.substring(1) : w);
        }
        if (addNumber) sb.append(rng.nextInt(10));
        char[] out = new char[sb.length()];
        sb.getChars(0, out.length, out, 0);
        sb.setLength(0);
        return out;
    }

    private static String filter(String s, boolean avoid) {
        if (!avoid) return s;
        StringBuilder b = new StringBuilder();
        for (char c : s.toCharArray()) if (AMBIGUOUS.indexOf(c) < 0) b.append(c);
        return b.toString();
    }
}
