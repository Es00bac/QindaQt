// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.io.Closeable;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Seam for part 2 (typing into the desktop). Given an entry id and a field choice it yields the
 * typing steps, only while the vault is unlocked, and zeroes every text buffer on {@link #close()}
 * or when the session locks. It adds no frames; part 2 specifies vault.* and desktop.*.
 *
 * Ownership: the caller closes the source (try-with-resources). Threading: the steps list is
 * immutable; close() may race with a reader, which then sees zeros or {@code LOCKED}.
 */
public final class VaultTypingSource implements Closeable {
    /** What to type. */
    public enum Field { USERNAME, PASSWORD, AUTOTYPE }

    private final VaultSession session;
    private List<TypingStep> steps;

    public VaultTypingSource(VaultSession session, String entryId, Field field) throws VaultException {
        this.session = session;
        VaultRecord r = session.get(entryId);   // throws LOCKED while locked
        try {
            List<TypingStep> out = new ArrayList<>();
            switch (field) {
                case USERNAME: out.add(TypingStep.text(r.username.getBytes(StandardCharsets.UTF_8))); break;
                case PASSWORD: out.add(TypingStep.text(r.password.clone())); break;
                default: build(r, out);
            }
            steps = Collections.unmodifiableList(out);
            session.register(this);
        } catch (VaultException e) {
            if (steps != null) close();
            throw e;
        } finally { r.wipe(); }
    }

    private static void build(VaultRecord r, List<TypingStep> out) throws VaultException {
        AutotypeSequence seq;
        try { seq = AutotypeSequence.parse(r.autotype); }
        catch (AutotypeSequence.SyntaxException e) { throw new VaultException(VaultException.Reason.INVALID, "autotype sequence is malformed"); }
        for (AutotypeSequence.Item it : seq.items()) {
            switch (it.kind) {
                case LITERAL: out.add(TypingStep.text(it.text.getBytes(StandardCharsets.UTF_8))); break;
                case USERNAME: out.add(TypingStep.text(r.username.getBytes(StandardCharsets.UTF_8))); break;
                case PASSWORD: out.add(TypingStep.text(r.password.clone())); break;
                case URL: out.add(TypingStep.text(r.url.getBytes(StandardCharsets.UTF_8))); break;
                case KEY: out.add(TypingStep.key(it.text)); break;
                default: out.add(TypingStep.delay(it.ms));
            }
        }
    }

    /** The steps; throws {@code LOCKED} once the vault locked or the source closed. */
    public List<TypingStep> steps() throws VaultException {
        List<TypingStep> s = steps;
        if (s == null || !session.isUnlocked()) throw new VaultException(VaultException.Reason.LOCKED, "vault is locked");
        return s;
    }

    @Override public void close() {
        List<TypingStep> s = steps;
        steps = null;
        if (s != null) for (TypingStep t : s) t.wipe();
        session.unregister(this);
    }
}
