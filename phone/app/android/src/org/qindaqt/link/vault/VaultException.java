// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

/**
 * Every vault failure. The message never contains vault content (titles, users, URLs, secrets);
 * callers may log {@link #reason()} but nothing else about the data.
 */
public final class VaultException extends Exception {
    private static final long serialVersionUID = 1L;
    /** Why an operation failed. */
    public enum Reason { LOCKED, CORRUPT, TAMPERED, UNSUPPORTED_VERSION, WRONG_PASSPHRASE, WEAK_PASSPHRASE, NOT_FOUND, LIMIT, INVALID,
        /** The key could not be used right now (auth failed, device locked, Keystore busy). Retryable; the vault is not damaged. */
        KEY_UNAVAILABLE,
        /** The key is permanently gone (lock screen removed or reset). The vault cannot be opened on this phone. */
        KEY_INVALIDATED,
        /** The file is older than a generation this phone already opened. */
        ROLLBACK }

    private final Reason reason;

    public VaultException(Reason reason, String message) { super(message); this.reason = reason; }
    public VaultException(Reason reason, String message, Throwable cause) { super(message, cause); this.reason = reason; }

    public Reason reason() { return reason; }
}
