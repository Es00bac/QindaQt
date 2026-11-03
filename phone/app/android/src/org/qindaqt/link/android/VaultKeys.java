// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.android;

import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import java.security.GeneralSecurityException;
import java.security.KeyStore;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import org.qindaqt.link.vault.DekWrapper;
import org.qindaqt.link.vault.KeyPresence;

/**
 * The AndroidKeyStore key-encryption key (KEK) that wraps the vault data key.
 *
 * AGENT-GUARD: the key requires user authentication for EVERY use (timeout 0, biometric strong or
 * device credential), so a Cipher from {@link #encryptCipher()} / {@link #decryptCipher} is useless
 * until it has been handed to BiometricPrompt as a CryptoObject; AAD and doFinal happen only after
 * success (see {@link Authorized}). Do not add a validity duration: that would let any code in the
 * process unwrap the DEK without a fresh prompt. The key is invalidated when the secure lock screen is
 * removed or reset. setInvalidatedByBiometricEnrollment(true) is requested, but because the device
 * credential is an allowed authenticator Android may not invalidate on a new fingerprint; do not
 * promise that to the user.
 * AGENT-GUARD: replacing the vault key must create the new key in the OTHER slot and delete the old
 * one only after the new vault file was written (VaultController.finishCreate); deleting first can
 * destroy a working vault when anything later fails.
 */
final class VaultKeys {
    /** Two alias slots so a replacement key can be created before the old one is deleted. */
    static final String[] ALIASES = {"qindalink-vault-kek", "qindalink-vault-kek-b"};
    private static final byte[] AAD = "qindalink-vault-dek-v1".getBytes(java.nio.charset.StandardCharsets.US_ASCII);
    private static final int IV_LEN = 12;

    private VaultKeys() {}

    /** Tri-state: a Keystore error is ERROR, never ABSENT (see {@link KeyPresence}). */
    static KeyPresence presence(int slot) {
        return KeyPresence.probe(() -> keyStore().containsAlias(ALIASES[slot]));
    }

    static void delete(int slot) {
        try { keyStore().deleteEntry(ALIASES[slot]); } catch (Exception ignored) { /* nothing to delete */ }
    }

    private static KeyStore keyStore() throws GeneralSecurityException, java.io.IOException {
        KeyStore ks = KeyStore.getInstance("AndroidKeyStore");
        ks.load(null);
        return ks;
    }

    private static KeyGenParameterSpec.Builder spec(int slot, boolean strongBox) {
        KeyGenParameterSpec.Builder b = new KeyGenParameterSpec.Builder(ALIASES[slot],
                KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256)
            .setUserAuthenticationRequired(true)
            .setUserAuthenticationParameters(0, KeyProperties.AUTH_BIOMETRIC_STRONG | KeyProperties.AUTH_DEVICE_CREDENTIAL)
            .setInvalidatedByBiometricEnrollment(true)
            .setUnlockedDeviceRequired(true);
        if (strongBox) b.setIsStrongBoxBacked(true);
        return b;
    }

    /** Creates the key in {@code slot} (replacing only a leftover in that same slot). True when StrongBox backed. */
    static boolean generate(int slot) throws GeneralSecurityException {
        delete(slot);
        try {
            generate(slot, true);
            return true;
        } catch (java.security.ProviderException e) {
            // No StrongBox (StrongBoxUnavailableException, or a plain ProviderException on some devices); the TEE fallback is
            // recorded in the vault header flags. A failure of the fallback propagates.
        }
        delete(slot);
        generate(slot, false);
        return false;
    }

    private static void generate(int slot, boolean strongBox) throws GeneralSecurityException {
        KeyGenerator g = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        g.init(spec(slot, strongBox).build());
        g.generateKey();
    }

    private static SecretKey key(int slot) throws GeneralSecurityException, java.io.IOException {
        KeyStore.Entry e = keyStore().getEntry(ALIASES[slot], null);
        if (!(e instanceof KeyStore.SecretKeyEntry)) throw new java.security.UnrecoverableKeyException("no key");
        return ((KeyStore.SecretKeyEntry) e).getSecretKey();
    }

    /** Cipher for wrapping a new DEK; throws if the key is gone. Needs authentication before use. */
    static Cipher encryptCipher(int slot) throws GeneralSecurityException, java.io.IOException {
        Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
        c.init(Cipher.ENCRYPT_MODE, key(slot));
        return c;
    }

    /**
     * Cipher for unwrapping. Throws {@link android.security.keystore.KeyPermanentlyInvalidatedException}
     * when a biometric was enrolled or the lock screen removed.
     */
    static Cipher decryptCipher(int slot, byte[] wrapped) throws GeneralSecurityException, java.io.IOException {
        if (wrapped.length <= IV_LEN) throw new GeneralSecurityException("short wrapped key");
        Cipher c = Cipher.getInstance("AES/GCM/NoPadding");
        c.init(Cipher.DECRYPT_MODE, key(slot), new GCMParameterSpec(128, Arrays.copyOf(wrapped, IV_LEN)));
        return c;
    }

    /** One-shot {@link DekWrapper} around a Cipher that BiometricPrompt has just authenticated. */
    static final class Authorized implements DekWrapper {
        private final Cipher cipher;
        Authorized(Cipher cipher) { this.cipher = cipher; }

        /** Keeps AEADBadTagException as is (the only "tampered" signal) and maps invalidation to its own type. */
        private static GeneralSecurityException map(GeneralSecurityException e) {
            return e instanceof android.security.keystore.KeyPermanentlyInvalidatedException
                ? new DekWrapper.KeyInvalidatedException("key invalidated") : e;
        }

        @Override public byte[] wrap(byte[] dek) throws GeneralSecurityException {
            try { return wrap0(dek); } catch (GeneralSecurityException e) { throw map(e); }
        }

        private byte[] wrap0(byte[] dek) throws GeneralSecurityException {
            cipher.updateAAD(AAD);
            byte[] ct = cipher.doFinal(dek);
            byte[] iv = cipher.getIV();
            byte[] out = Arrays.copyOf(iv, iv.length + ct.length);
            System.arraycopy(ct, 0, out, iv.length, ct.length);
            return out;
        }

        @Override public byte[] unwrap(byte[] wrapped) throws GeneralSecurityException {
            try {
                cipher.updateAAD(AAD);
                return cipher.doFinal(wrapped, IV_LEN, wrapped.length - IV_LEN);
            } catch (GeneralSecurityException e) { throw map(e); }
        }
    }
}
