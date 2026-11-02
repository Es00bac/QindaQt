package org.qindaqt.link.android;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.provider.ContactsContract;
import java.util.LinkedHashMap;
import java.util.Map;
import org.qindaqt.link.sms.ContactResolver;

/**
 * Resolves a number to a contact display name with READ_CONTACTS. Returns "" without the permission, for
 * non-numeric senders and on any provider failure. Names live only in a small in-memory cache and are never logged.
 */
public final class ContactsResolver implements ContactResolver {
    private static final int CACHE = 512;
    private final Context ctx;
    private final Map<String, String> cache = new LinkedHashMap<String, String>(64, 0.75f, true) {
        private static final long serialVersionUID = 1L;
        @Override protected boolean removeEldestEntry(Map.Entry<String, String> e) { return size() > CACHE; }
    };

    public ContactsResolver(Context ctx) { this.ctx = ctx.getApplicationContext(); }

    @Override public synchronized String name(String address) {
        if (address == null || address.isEmpty()
                || ctx.checkSelfPermission(Manifest.permission.READ_CONTACTS) != PackageManager.PERMISSION_GRANTED) return "";
        String hit = cache.get(address);
        if (hit != null) return hit;
        String name = "";
        boolean digit = false;
        for (int i = 0; i < address.length(); i++) if (Character.isDigit(address.charAt(i))) { digit = true; break; }
        if (digit) {
            Uri u = Uri.withAppendedPath(ContactsContract.PhoneLookup.CONTENT_FILTER_URI, Uri.encode(address));
            try (Cursor c = ctx.getContentResolver().query(u, new String[] {ContactsContract.PhoneLookup.DISPLAY_NAME}, null, null, null)) {
                if (c != null && c.moveToFirst() && !c.isNull(0)) name = c.getString(0);
            } catch (RuntimeException e) {
                name = "";
            }
        }
        cache.put(address, name);
        return name;
    }
}
