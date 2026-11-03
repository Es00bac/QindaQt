// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.security.GeneralSecurityException;

/**
 * Wraps and unwraps the vault data key (DEK) with a key-encryption key that this package never sees.
 *
 * AGENT-CONTRACT: on Android the implementation is a single-use object holding a Cipher that the
 * user has just authenticated through BiometricPrompt (VaultKeys in android/); in JVM tests it is
 * an in-memory AES-GCM key. Pure Java must never import Android classes.
 * Ownership: the arguments belong to the caller; implementations must not retain them.
 */
public interface DekWrapper {
    /**
     * Thrown by {@link #wrap}/{@link #unwrap} when the key is permanently unusable (Android:
     * KeyPermanentlyInvalidatedException). Any other GeneralSecurityException except
     * AEADBadTagException means "could not use the key now" and is retryable; only AEADBadTagException
     * means the wrapped blob itself failed authentication.
     */
    final class KeyInvalidatedException extends GeneralSecurityException {
        private static final long serialVersionUID = 1L;
        public KeyInvalidatedException(String m) { super(m); }
    }

    /** Returns an opaque blob that {@link #unwrap} turns back into the DEK. */
    byte[] wrap(byte[] dek) throws GeneralSecurityException;

    /** Returns the DEK (the caller zeroes it); throws if the blob was altered or the key is wrong. */
    byte[] unwrap(byte[] wrapped) throws GeneralSecurityException;
}
