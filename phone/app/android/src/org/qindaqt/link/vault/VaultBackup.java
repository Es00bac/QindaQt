// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.security.GeneralSecurityException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;

/**
 * Encrypted backup file; there is no plaintext export anywhere.
 *
 * <pre>
 * "QLVB" | u16 version=1 | u8 kdf=1 (PBKDF2-HMAC-SHA256) | u32 iterations | salt[16] | iv[12] | u32 len | ct
 * plaintext = u32 n, then n x VaultRecord
 * </pre>
 *
 * AAD is every header byte before the IV, so the iteration count and salt are authenticated: an
 * attacker cannot lower the iteration count without failing the tag. The key is derived from a
 * user passphrase; a wrong passphrase and a tampered file are both reported as WRONG_PASSPHRASE
 * because AES-GCM cannot tell them apart (a damaged header is reported as CORRUPT).
 */
public final class VaultBackup {
    public static final int VERSION = 1, MIN_ITERATIONS = 600_000, MAX_ITERATIONS = 10_000_000, SALT_LEN = 16;
    private static final byte[] MAGIC = {'Q', 'L', 'V', 'B'};

    private VaultBackup() {}

    /**
     * Picks an iteration count for roughly {@code targetMillis} on this device, clamped to
     * [MIN_ITERATIONS, MAX_ITERATIONS]. The Android layer calls it once per export and the chosen
     * count is stored in the file header.
     */
    public static int calibrate(long targetMillis) {
        final int probe = 50_000;
        try { derive("calibrate".toCharArray(), new byte[SALT_LEN], 1000); } catch (VaultException ignored) { return MIN_ITERATIONS; }
        long t0 = System.nanoTime();
        try { derive("calibrate".toCharArray(), new byte[SALT_LEN], probe); } catch (VaultException ignored) { return MIN_ITERATIONS; }
        double perIter = Math.max(1, System.nanoTime() - t0) / (double) probe;
        long n = (long) (targetMillis * 1_000_000.0 / perIter);
        return (int) Math.max(MIN_ITERATIONS, Math.min(MAX_ITERATIONS, n));
    }

    public static byte[] export(List<VaultRecord> records, char[] passphrase, int iterations) throws VaultException {
        if (iterations < MIN_ITERATIONS || iterations > MAX_ITERATIONS) throw new VaultException(VaultException.Reason.INVALID, "iterations");
        if (!PassphraseStrength.check(passphrase).ok) throw new VaultException(VaultException.Reason.WEAK_PASSPHRASE, "passphrase too short");
        byte[] salt = VaultCrypto.random(SALT_LEN);
        byte[] header = new Bin.Out().raw(MAGIC).u16(VERSION).u8(1).u32(iterations).raw(salt).toByteArray();
        Bin.Out p = new Bin.Out().u32(records.size());
        for (VaultRecord r : records) { byte[] b = r.toBytes(); p.raw(b); Bin.wipe(b); }
        byte[] plain = p.toByteArray();
        byte[] key = derive(passphrase, salt, iterations);
        try {
            VaultCrypto.Sealed s = VaultCrypto.seal(key, header, plain);
            return new Bin.Out().raw(header).raw(s.iv).bytes(s.ct).toByteArray();
        } finally { Bin.wipe(key); Bin.wipe(plain); }
    }

    /** The iteration count in a backup header (for display), after validation. */
    public static int iterationsOf(byte[] file) throws VaultException { return parse(file).iterations; }

    /** Decrypts a backup. The caller wipes the returned records. */
    public static List<VaultRecord> read(byte[] file, char[] passphrase) throws VaultException {
        Parsed h = parse(file);
        byte[] key = derive(passphrase, h.salt, h.iterations);
        byte[] plain;
        try {
            try { plain = VaultCrypto.open(key, h.header, h.iv, h.ct); }
            catch (VaultException e) { throw new VaultException(VaultException.Reason.WRONG_PASSPHRASE, "wrong passphrase or damaged file"); }
        } finally { Bin.wipe(key); }
        List<VaultRecord> out = new ArrayList<>();
        try {
            Bin.In in = new Bin.In(plain);
            long n = in.u32();
            if (n > VaultFormat.MAX_ENTRIES) throw new VaultException(VaultException.Reason.CORRUPT, "too many entries");
            for (long i = 0; i < n; i++) out.add(VaultRecord.read(in));
            if (in.remaining() != 0) throw new VaultException(VaultException.Reason.CORRUPT, "trailing bytes");
            return out;
        } catch (VaultException e) {
            for (VaultRecord r : out) r.wipe();
            throw e;
        } finally { Bin.wipe(plain); }
    }

    private static final class Parsed {
        byte[] header, salt, iv, ct;
        int iterations;
    }

    private static Parsed parse(byte[] file) throws VaultException {
        Bin.In in = new Bin.In(file);
        if (!Arrays.equals(in.raw(4), MAGIC)) throw new VaultException(VaultException.Reason.CORRUPT, "not a backup");
        if (in.u16() != VERSION) throw new VaultException(VaultException.Reason.UNSUPPORTED_VERSION, "backup version");
        if (in.u8() != 1) throw new VaultException(VaultException.Reason.UNSUPPORTED_VERSION, "kdf");
        Parsed p = new Parsed();
        long it = in.u32();
        if (it < MIN_ITERATIONS || it > MAX_ITERATIONS) throw new VaultException(VaultException.Reason.CORRUPT, "iteration count");
        p.iterations = (int) it;
        p.salt = in.raw(SALT_LEN);
        p.header = Arrays.copyOf(file, in.pos());
        p.iv = in.raw(VaultCrypto.IV_LEN);
        p.ct = in.bytes(VaultFormat.MAX_CT * 8);
        if (in.remaining() != 0) throw new VaultException(VaultException.Reason.CORRUPT, "trailing bytes");
        return p;
    }

    private static byte[] derive(char[] pass, byte[] salt, int iterations) throws VaultException {
        PBEKeySpec spec = new PBEKeySpec(pass, salt, iterations, 256);
        try {
            return SecretKeyFactory.getInstance("PBKDF2WithHmacSHA256").generateSecret(spec).getEncoded();
        } catch (GeneralSecurityException e) {
            throw new VaultException(VaultException.Reason.INVALID, "kdf unavailable", e);
        } finally { spec.clearPassword(); }
    }

}
