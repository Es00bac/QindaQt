// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.nio.charset.StandardCharsets;
import java.util.Arrays;

/**
 * A whole entry including its secrets. The password and notes are byte arrays (UTF-8) so
 * {@link #wipe()} can zero them. Whoever obtains a record from the session owns it and must call
 * wipe() when done.
 */
public final class VaultRecord {
    /** Default autotype sequence; grammar in {@link AutotypeSequence}. */
    public static final String DEFAULT_AUTOTYPE = "{USERNAME}{TAB}{PASSWORD}{ENTER}";
    static final int MAX_FIELD = 64 * 1024;

    public String id;               // null for a new entry; the session assigns it
    public String title = "", username = "", url = "", autotype = DEFAULT_AUTOTYPE;
    public long updatedMillis;
    public byte[] password = new byte[0];
    public byte[] notes = new byte[0];

    public VaultRecord() {}

    public VaultRecord setPassword(String s) { Bin.wipe(password); password = s.getBytes(StandardCharsets.UTF_8); return this; }
    public VaultRecord setNotes(String s) { Bin.wipe(notes); notes = s.getBytes(StandardCharsets.UTF_8); return this; }
    public String passwordString() { return new String(password, StandardCharsets.UTF_8); }
    public String notesString() { return new String(notes, StandardCharsets.UTF_8); }

    public void wipe() { Bin.wipe(password); Bin.wipe(notes); password = new byte[0]; notes = new byte[0]; }

    VaultEntry listFields() { return new VaultEntry(id, title, username, url, updatedMillis); }

    /** Serialised form shared by the vault file and the backup. The caller wipes the result. */
    byte[] toBytes() {
        Bin.Out o = new Bin.Out();
        o.u8(1).str(id).str(title).str(username).str(url).u64(updatedMillis).bytes(password).bytes(notes).str(autotype);
        return o.toByteArray();
    }

    static VaultRecord read(Bin.In in) throws VaultException {
        if (in.u8() != 1) throw new VaultException(VaultException.Reason.UNSUPPORTED_VERSION, "record version");
        VaultRecord r = new VaultRecord();
        r.id = in.str(128); r.title = in.str(MAX_FIELD); r.username = in.str(MAX_FIELD); r.url = in.str(MAX_FIELD);
        r.updatedMillis = in.u64();
        r.password = in.bytes(MAX_FIELD); r.notes = in.bytes(MAX_FIELD); r.autotype = in.str(MAX_FIELD);
        return r;
    }

    static VaultRecord fromBytes(byte[] b) throws VaultException {
        Bin.In in = new Bin.In(b);
        VaultRecord r = read(in);
        if (in.remaining() != 0) { r.wipe(); throw new VaultException(VaultException.Reason.CORRUPT, "trailing bytes"); }
        return r;
    }

    @Override public String toString() { return "VaultRecord[redacted]"; }

    boolean contentEquals(VaultRecord o) {
        return Arrays.equals(password, o.password) && Arrays.equals(notes, o.notes) && title.equals(o.title)
            && username.equals(o.username) && url.equals(o.url) && autotype.equals(o.autotype);
    }
}
