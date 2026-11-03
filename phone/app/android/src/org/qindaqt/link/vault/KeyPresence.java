// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.util.concurrent.Callable;

/**
 * Tri-state answer to "is the Keystore key there?". An exception while asking is ERROR, which is
 * NOT the same as ABSENT: a transient Keystore hiccup must never be read as a lost key.
 *
 * AGENT-GUARD: only a definite ABSENT (or a definite KeyPermanentlyInvalidatedException) may lead
 * to the INVALIDATED state; see {@link VaultLifecycle#decideLocked}.
 */
public enum KeyPresence {
    PRESENT, ABSENT, ERROR;

    /** Runs {@code check}; true is PRESENT, false is ABSENT, anything thrown is ERROR. */
    public static KeyPresence probe(Callable<Boolean> check) {
        try { return check.call() ? PRESENT : ABSENT; } catch (Exception | LinkageError e) { return ERROR; }
    }

    /** Presence across several slots when the file does not say which one it uses: any PRESENT wins, then any ERROR. */
    public static KeyPresence any(KeyPresence a, KeyPresence b) {
        if (a == PRESENT || b == PRESENT) return PRESENT;
        if (a == ERROR || b == ERROR) return ERROR;
        return ABSENT;
    }
}
