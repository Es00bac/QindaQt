// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

/**
 * Pure state rules of the vault lifecycle, shared by the Android controller and the JVM tests.
 *
 * AGENT-GUARD: recovery actions (restore, erase) may only be offered for states where the vault is
 * known to be unusable. A retryable failure (authentication, locked device, Keystore busy) must map
 * to LOCKED, never to ERROR: ERROR offers "Restore from backup", and a restore replaces the key and
 * the file. Mapping a transient failure to ERROR is how a working vault gets destroyed.
 */
public final class VaultLifecycle {
    /** Lifecycle shown to the UI; lower-case names are the QML contract. */
    public enum State { NONE, LOCKED, UNLOCKED, AUTHENTICATING, INVALIDATED, NOLOCK, ERROR, RESTORE_READY }

    private VaultLifecycle() {}

    /** State after an unlock attempt failed with {@code reason}. */
    public static State afterOpenFailure(VaultException.Reason reason) {
        switch (reason) {
            case TAMPERED: case CORRUPT: case UNSUPPORTED_VERSION: case ROLLBACK: return State.ERROR;
            case KEY_INVALIDATED: return State.INVALIDATED;
            default: return State.LOCKED;      // KEY_UNAVAILABLE and anything unexpected: retry, no recovery buttons
        }
    }

    /** Sentinel for "no slot has been marked invalidated". */
    public static final int NO_SLOT = -9;

    /** What the controller does for an existing vault file at rest. */
    public enum Decision { LOCKED, INVALIDATED, KEY_STORE_RETRY }

    /**
     * Decides the resting state of an existing vault file.
     * @param fileSlot slot named by the file header, -1 if unreadable
     * @param presence presence of the key for that slot (any slot when unknown)
     * @param invalidatedSlot slot persisted as invalidated, or {@link #NO_SLOT}
     *
     * AGENT-GUARD: a stored flag only counts for the slot it was recorded for, so a flag left over from a
     * previous vault (crash between writing a new file and clearing the flag) cannot strand the new one;
     * KeyPresence.ERROR never invalidates, it is a retry.
     */
    public static Decision decideLocked(int fileSlot, KeyPresence presence, int invalidatedSlot) {
        if (invalidatedSlot != NO_SLOT && invalidatedSlot == fileSlot) return Decision.INVALIDATED;
        switch (presence) {
            case ABSENT: return Decision.INVALIDATED;
            case ERROR: return Decision.KEY_STORE_RETRY;
            default: return Decision.LOCKED;
        }
    }

    /** A backup may only be restored over a vault that is absent or known unusable. */
    public static boolean canRestore(State s) { return s == State.NONE || s == State.INVALIDATED || s == State.ERROR; }

    /** Restoring over an existing (unusable) vault needs the explicit "replaces the existing vault" confirmation. */
    public static boolean restoreNeedsReplaceConfirm(State s) { return s == State.INVALIDATED || s == State.ERROR; }

    /** createVault is only reachable fresh or as the second half of a restore. */
    public static boolean canCreate(State s) { return s == State.NONE || s == State.RESTORE_READY; }

    /** Erasing is destructive and allowed only for unusable vaults or an open one. */
    public static boolean canErase(State s) { return s == State.INVALIDATED || s == State.ERROR || s == State.UNLOCKED; }

    /** Key alias slot to create next, given the slot used by the existing file (-1 unknown) and which slots exist. */
    public static int nextKeySlot(int existingFileSlot, boolean slot0Exists) {
        if (existingFileSlot >= 0) return 1 - existingFileSlot;
        return slot0Exists ? 1 : 0;
    }
}
