// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.util.HashSet;
import java.util.Set;

/**
 * Backup passphrase rules and a deliberately rough strength estimate. Accepted when the estimate is
 * at least "fair" (40 bits) and the passphrase has either at least 12 characters or at least 4 words
 * (whitespace separated, each with at least 4 letters or digits).
 */
public final class PassphraseStrength {
    public static final int MIN_CHARS = 12, MIN_WORDS = 4, MIN_WORD_LETTERS = 4, MIN_BITS = 40;

    /** The verdict. {@code ok} is the only gating field; label and bits are advice. */
    public static final class Result {
        public final boolean ok;
        public final String label, reason;
        public final int bits;
        Result(boolean ok, String label, String reason, int bits) { this.ok = ok; this.label = label; this.reason = reason; this.bits = bits; }
    }

    private PassphraseStrength() {}

    public static Result check(char[] p) {
        int words = 0, run = 0;
        Set<Character> uniq = new HashSet<>();
        boolean lo = false, up = false, di = false, sy = false;
        for (char c : p) {
            if (Character.isWhitespace(c)) { if (run >= MIN_WORD_LETTERS) words++; run = 0; continue; }
            run++;
            uniq.add(c);
            if (Character.isLowerCase(c)) lo = true; else if (Character.isUpperCase(c)) up = true;
            else if (Character.isDigit(c)) di = true; else sy = true;
        }
        if (run >= MIN_WORD_LETTERS) words++;
        int pool = (lo ? 26 : 0) + (up ? 26 : 0) + (di ? 10 : 0) + (sy ? 30 : 0);
        double bits = pool == 0 ? 0 : Math.min(p.length, uniq.size() * 2.0) * Math.log(pool) / Math.log(2);
        if (words >= MIN_WORDS && !up && !di && !sy) bits = Math.max(bits, words * 11.0); // words from a large list
        int b = (int) bits;
        String label = b < MIN_BITS ? "weak" : b < 60 ? "fair" : b < 80 ? "good" : "strong";
        boolean shapeOk = p.length >= MIN_CHARS || words >= MIN_WORDS;
        boolean ok = shapeOk && b >= MIN_BITS;
        String reason = ok ? "" : !shapeOk ? "Use at least 12 characters or 4 words of 4+ letters"
            : "Too predictable: mix more different characters or use more words";
        return new Result(ok, label, reason, b);
    }
}
