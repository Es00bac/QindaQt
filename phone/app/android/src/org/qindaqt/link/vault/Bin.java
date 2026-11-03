// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/** Big-endian length-prefixed binary reader/writer used by the vault and backup formats. */
final class Bin {
    private Bin() {}

    /** Growing writer. {@link #wipe()} zeroes the internal buffer copy where possible. */
    static final class Out {
        private final ByteArrayOutputStream bos = new ByteArrayOutputStream();
        Out u8(int v) { bos.write(v); return this; }
        Out u16(int v) { bos.write(v >>> 8); bos.write(v); return this; }
        Out u32(long v) { u16((int) (v >>> 16)); u16((int) v); return this; }
        Out u64(long v) { u32(v >>> 32); u32(v & 0xffffffffL); return this; }
        Out raw(byte[] b) { bos.write(b, 0, b.length); return this; }
        Out bytes(byte[] b) { u32(b.length); return raw(b); }
        Out str(String s) { return bytes(s.getBytes(StandardCharsets.UTF_8)); }
        byte[] toByteArray() { return bos.toByteArray(); }
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
