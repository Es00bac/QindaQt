// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

/** The non-secret list fields of an entry. Still encrypted at rest (inside the index record). */
public final class VaultEntry {
    public final String id, title, username, url;
    public final long updatedMillis;

    public VaultEntry(String id, String title, String username, String url, long updatedMillis) {
        this.id = id; this.title = title; this.username = username; this.url = url; this.updatedMillis = updatedMillis;
    }

    boolean sameListFields(VaultEntry o) {
        return id.equals(o.id) && title.equals(o.title) && username.equals(o.username) && url.equals(o.url)
            && updatedMillis == o.updatedMillis;
    }

    /** Case-insensitive match on the three list fields only. */
    boolean matches(String lowerQuery) {
        return title.toLowerCase().contains(lowerQuery) || username.toLowerCase().contains(lowerQuery)
            || url.toLowerCase().contains(lowerQuery);
    }
}
