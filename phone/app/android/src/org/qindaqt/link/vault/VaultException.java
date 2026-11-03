// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

/**
 * Every vault failure. The message never contains vault content (titles, users, URLs, secrets);
 * callers may log {@link #reason()} but nothing else about the data.
 */
public final class VaultException extends Exception {
    private static final long serialVersionUID = 1L;
    /** Why an operation failed. */
    public enum Reason { LOCKED, CORRUPT, TAMPERED, UNSUPPORTED_VERSION, WRONG_PASSPHRASE, WEAK_PASSPHRASE, NOT_FOUND, LIMIT, INVALID }

    private final Reason reason;

    public VaultException(Reason reason, String message) { super(message); this.reason = reason; }
    public VaultException(Reason reason, String message, Throwable cause) { super(message, cause); this.reason = reason; }

    public Reason reason() { return reason; }
}
