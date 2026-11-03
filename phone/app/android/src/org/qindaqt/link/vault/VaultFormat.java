// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * On-disk layout of the vault file (all integers big-endian):
 *
 * <pre>
 * header : "QLVT" | u16 version=1 | u8 flags (bit0 = StrongBox key) | u16 wrappedLen | wrapped DEK
 * index  : iv[12] | u32 len | ct          (AES-GCM; plaintext = u32 n, then n x list fields)
 * entries: u32 n, then n x ( u8 idLen | id | iv[12] | u32 len | ct )   (plaintext = VaultRecord)
 * </pre>
 *
 * AGENT-GUARD: AAD for an entry is "QLVT-entry" + id + u16 version; for the index it is "QLVT-index"
 * + the exact header bytes. So a flipped header byte, a swapped id or a changed version fails the
 * GCM tag (TAMPERED) and nothing is parsed from unauthenticated data. Never decode before open().
 * Deviation from the lane brief, recorded in ADR-0438: the index AAD also covers the header.
 */
final class VaultFormat {
    static final byte[] MAGIC = {'Q', 'L', 'V', 'T'};
    static final int VERSION = 1;
    static final int MAX_ENTRIES = 50_000, MAX_CT = 4 * 1024 * 1024, MAX_WRAPPED = 512;
    static final int FLAG_STRONGBOX = 1;

    private VaultFormat() {}

    /** The parsed, still unauthenticated header. {@code bytes} is exactly what the index AAD covers. */
    static final class Header {
        final int flags;
        final byte[] wrapped, bytes;
        final int bodyOffset;
        Header(int flags, byte[] wrapped, byte[] bytes) {
            this.flags = flags; this.wrapped = wrapped; this.bytes = bytes; this.bodyOffset = bytes.length;
        }
    }

    static byte[] headerBytes(int flags, byte[] wrapped) {
        return new Bin.Out().raw(MAGIC).u16(VERSION).u8(flags).u16(wrapped.length).raw(wrapped).toByteArray();
    }

    static Header parseHeader(byte[] file) throws VaultException {
        Bin.In in = new Bin.In(file);
        byte[] magic = in.raw(4);
        if (!java.util.Arrays.equals(magic, MAGIC)) throw new VaultException(VaultException.Reason.CORRUPT, "not a vault");
        int version = in.u16();
        if (version != VERSION) throw new VaultException(VaultException.Reason.UNSUPPORTED_VERSION, "vault version");
        int flags = in.u8();
        int n = in.u16();
        if (n == 0 || n > MAX_WRAPPED) throw new VaultException(VaultException.Reason.CORRUPT, "wrapped key size");
        byte[] wrapped = in.raw(n);
        return new Header(flags, wrapped, java.util.Arrays.copyOf(file, in.pos()));
    }

    static byte[] entryAad(String id) {
        return new Bin.Out().raw("QLVT-entry".getBytes(StandardCharsets.US_ASCII)).str(id).u16(VERSION).toByteArray();
    }

    static byte[] indexAad(byte[] header) {
        return new Bin.Out().raw("QLVT-index".getBytes(StandardCharsets.US_ASCII)).raw(header).toByteArray();
    }

    static byte[] encodeIndex(Map<String, VaultEntry> index) {
        Bin.Out o = new Bin.Out().u32(index.size());
        for (VaultEntry e : index.values()) o.str(e.id).str(e.title).str(e.username).str(e.url).u64(e.updatedMillis);
        return o.toByteArray();
    }

    static Map<String, VaultEntry> decodeIndex(byte[] plain) throws VaultException {
        Bin.In in = new Bin.In(plain);
        long n = in.u32();
        if (n > MAX_ENTRIES) throw new VaultException(VaultException.Reason.CORRUPT, "too many entries");
        Map<String, VaultEntry> m = new LinkedHashMap<>();
        for (long i = 0; i < n; i++) {
            VaultEntry e = new VaultEntry(in.str(128), in.str(VaultRecord.MAX_FIELD), in.str(VaultRecord.MAX_FIELD),
                in.str(VaultRecord.MAX_FIELD), 0);
            e = new VaultEntry(e.id, e.title, e.username, e.url, in.u64());
            if (m.put(e.id, e) != null) throw new VaultException(VaultException.Reason.CORRUPT, "duplicate id");
        }
        if (in.remaining() != 0) throw new VaultException(VaultException.Reason.CORRUPT, "trailing bytes");
        return m;
    }
}
