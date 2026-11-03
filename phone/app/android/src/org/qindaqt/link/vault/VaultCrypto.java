// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.security.GeneralSecurityException;
import java.security.SecureRandom;
import javax.crypto.Cipher;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/** AES-256-GCM sealing with a fresh random 96-bit IV per call; failures are {@code TAMPERED}. */
final class VaultCrypto {
    static final int IV_LEN = 12, TAG_BITS = 128, KEY_LEN = 32;
    private static final SecureRandom RNG = new SecureRandom();

    private VaultCrypto() {}

    /** An IV and the ciphertext||tag it produced. */
    static final class Sealed {
        final byte[] iv, ct;
        Sealed(byte[] iv, byte[] ct) { this.iv = iv; this.ct = ct; }
    }

    static byte[] random(int n) { byte[] r = new byte[n]; RNG.nextBytes(r); return r; }

    static Sealed seal(byte[] key, byte[] aad, byte[] plain) throws VaultException {
        try {
            byte[] iv = random(IV_LEN);
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.ENCRYPT_MODE, new SecretKeySpec(key, "AES"), new GCMParameterSpec(TAG_BITS, iv));
            c.updateAAD(aad);
            return new Sealed(iv, c.doFinal(plain));
        } catch (GeneralSecurityException e) {
            throw new VaultException(VaultException.Reason.INVALID, "seal failed", e);
        }
    }

    /** Fails closed: a changed key, IV, AAD or ciphertext byte yields {@code TAMPERED}. */
    static byte[] open(byte[] key, byte[] aad, byte[] iv, byte[] ct) throws VaultException {
        if (iv.length != IV_LEN || ct.length < TAG_BITS / 8) throw new VaultException(VaultException.Reason.CORRUPT, "bad record");
        try {
            Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
            c.init(Cipher.DECRYPT_MODE, new SecretKeySpec(key, "AES"), new GCMParameterSpec(TAG_BITS, iv));
            c.updateAAD(aad);
            return c.doFinal(ct);
        } catch (GeneralSecurityException e) {
            throw new VaultException(VaultException.Reason.TAMPERED, "authentication failed");
        }
    }
}
