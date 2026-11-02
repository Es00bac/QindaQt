package org.qindaqt.link.proto;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.Arrays;

/**
 * Commit/reveal pairing math from the protocol page. Pure Java.
 *
 * AGENT-CONTRACT: labels and concatenation order must equal the C++ side
 * (tests/link/vectors/pairing-v1.json). spkiD is the desktop digest, spkiP the
 * phone digest, each the raw 32-byte SHA-256 of the DER SubjectPublicKeyInfo.
 */
public final class Pairing {
    private Pairing() {}

    private static byte[] sha(byte[]... parts) {
        try {
            MessageDigest d = MessageDigest.getInstance("SHA-256");
            for (byte[] p : parts) d.update(p);
            return d.digest();
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException(e);
        }
    }

    private static byte[] ascii(String s) { return s.getBytes(java.nio.charset.StandardCharsets.US_ASCII); }

    public static byte[] commitment(byte[] nd, byte[] spkiD, byte[] spkiP) {
        return sha(ascii("QLINK-COMMIT-1"), nd, spkiD, spkiP);
    }

    /** Constant-time-ish comparison of the received commitment with the recomputed one. */
    public static boolean verifyCommitment(byte[] c, byte[] nd, byte[] spkiD, byte[] spkiP) {
        return MessageDigest.isEqual(c, commitment(nd, spkiD, spkiP));
    }

    /** Six-digit code, no separator. */
    public static String code(byte[] spkiD, byte[] spkiP, byte[] nd, byte[] np) {
        byte[] h = sha(ascii("QLINK-SAS-1"), spkiD, spkiP, nd, np);
        long v = ((h[0] & 0xffL) << 24) | ((h[1] & 0xff) << 16) | ((h[2] & 0xff) << 8) | (h[3] & 0xff);
        return String.format(java.util.Locale.ROOT, "%06d", v % 1000000L);
    }

    /** "123 456" display form. */
    public static String display(String code) { return code.substring(0, 3) + " " + code.substring(3); }

    public static String hex(byte[] b) {
        char[] d = "0123456789abcdef".toCharArray();
        StringBuilder s = new StringBuilder(b.length * 2);
        for (byte x : b) s.append(d[(x >> 4) & 15]).append(d[x & 15]);
        return s.toString();
    }

    private static int nib(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }

    /** Strict lowercase hex (protocol rule: no uppercase, no non-ASCII digits) of exactly {@code bytes} bytes; null if malformed. */
    public static byte[] unhex(String s, int bytes) {
        if (s == null || s.length() != bytes * 2) return null;
        byte[] o = new byte[bytes];
        for (int i = 0; i < bytes; i++) {
            int h = nib(s.charAt(2 * i)), l = nib(s.charAt(2 * i + 1));
            if (h < 0 || l < 0) return null;
            o[i] = (byte) (h * 16 + l);
        }
        return o;
    }

    static boolean same(byte[] a, byte[] b) { return Arrays.equals(a, b); }
}
