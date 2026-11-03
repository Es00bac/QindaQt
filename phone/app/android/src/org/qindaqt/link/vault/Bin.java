// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Big-endian length-prefixed binary reader/writer used by the vault and backup formats. */
final class Bin {
    private Bin() {}

    /**
     * Growing writer over its own buffer so {@link #wipe()} can really zero it (a ByteArrayOutputStream
     * would leave old copies behind when it grows; this one zeroes the old buffer on every growth).
     * AGENT-GUARD: call wipe() on every Out that carried a secret, after toByteArray().
     */
    static final class Out {
        private byte[] buf = new byte[256];
        private int n;
        private void room(int add) {
            if (n + add <= buf.length) return;
            byte[] bigger = new byte[Math.max(buf.length * 2, n + add)];
            System.arraycopy(buf, 0, bigger, 0, n);
            Arrays.fill(buf, (byte) 0);
            buf = bigger;
        }
        private void put(int v) { room(1); buf[n++] = (byte) v; }
        Out u8(int v) { put(v); return this; }
        Out u16(int v) { put(v >>> 8); put(v); return this; }
        Out u32(long v) { u16((int) (v >>> 16)); u16((int) v); return this; }
        Out u64(long v) { u32(v >>> 32); u32(v & 0xffffffffL); return this; }
        Out raw(byte[] b) { room(b.length); System.arraycopy(b, 0, buf, n, b.length); n += b.length; return this; }
        Out bytes(byte[] b) { u32(b.length); return raw(b); }
        Out str(String s) { return bytes(s.getBytes(StandardCharsets.UTF_8)); }
        byte[] toByteArray() { return Arrays.copyOf(buf, n); }
        /** Zeroes the internal buffer and empties the writer. */
        void wipe() { Arrays.fill(buf, (byte) 0); n = 0; }
    }

    /** Bounds-checked reader; every failure is {@code CORRUPT}. */
    static final class In {
        private final byte[] b;
        private int p;
        In(byte[] b) { this.b = b; }
        int pos() { return p; }
        int remaining() { return b.length - p; }
        private void need(long n) throws VaultException {
            if (n < 0 || n > b.length - p) throw new VaultException(VaultException.Reason.CORRUPT, "truncated");
        }
        int u8() throws VaultException { need(1); return b[p++] & 0xff; }
        int u16() throws VaultException { return (u8() << 8) | u8(); }
        long u32() throws VaultException { return ((long) u16() << 16) | u16(); }
        long u64() throws VaultException { return (u32() << 32) | u32(); }
        byte[] raw(int n) throws VaultException { need(n); byte[] r = Arrays.copyOfRange(b, p, p + n); p += n; return r; }
        byte[] bytes(int max) throws VaultException {
            long n = u32();
            if (n > max) throw new VaultException(VaultException.Reason.CORRUPT, "field too large");
            return raw((int) n);
        }
        String str(int max) throws VaultException {
            byte[] r = bytes(max);
            String s = new String(r, StandardCharsets.UTF_8);
            Arrays.fill(r, (byte) 0);
            return s;
        }
    }

    static void wipe(byte[] b) { if (b != null) Arrays.fill(b, (byte) 0); }
}
