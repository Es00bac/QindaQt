package org.qindaqt.link.sms;

/**
 * Phone-side defence for sms.send: only dialable numbers are accepted. Formatting characters (space, dash,
 * dot, parentheses) are stripped; what remains must be digits with an optional leading '+' and at least
 * 7 digits. That excludes premium short codes (3-6 digits) and alphanumeric sender ids.
 *
 * AGENT-GUARD: this check is the last line before a real, possibly billable, SMS. Do not loosen it to
 * accept short codes without an explicit user decision and a protocol page change.
 */
public final class Dialable {
    public static final int MIN_DIGITS = 7, MAX_DIGITS = 20;

    private Dialable() { }

    /** The normalized number, or null when {@code raw} is not an acceptable destination. */
    public static String normalize(String raw) {
        if (raw == null) return null;
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < raw.length(); i++) {
            char c = raw.charAt(i);
            if (c == ' ' || c == '-' || c == '.' || c == '(' || c == ')') continue;
            if (c == '+' && sb.length() == 0) { sb.append(c); continue; }
            if (c < '0' || c > '9') return null; // ASCII digits only
            sb.append(c);
        }
        int digits = sb.length() - (sb.length() > 0 && sb.charAt(0) == '+' ? 1 : 0);
        return digits >= MIN_DIGITS && digits <= MAX_DIGITS ? sb.toString() : null;
    }
}
