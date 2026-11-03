package org.qindaqt.link.android;

import android.Manifest;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Callable;
import org.qindaqt.link.sms.FallbackChain;
import org.qindaqt.link.sms.SmsProvider;
import org.qindaqt.link.sms.SmsQueries;

/**
 * SmsProvider over content://mms-sms, content://sms and content://mms (needs READ_SMS).
 *
 * AGENT-GUARD: never log a cursor value, address, snippet or body; this class handles the user's real messages.
 * AGENT-NOTE: written for the AOSP/Samsung provider schema: sms.date and mms-sms conversation dates are ms,
 * mms.date is seconds. Optional enrichments degrade (logged by exception class); a failed list query throws ProviderException so the desktop gets error{internal}, never a silent empty list.
 */
public final class SmsProviderAndroid implements SmsProvider {
    private static final Uri CONVERSATIONS = Uri.parse("content://mms-sms/conversations?simple=true");
    private static final Uri CANONICAL = Uri.parse("content://mms-sms/canonical-addresses");
    private static final Uri SMS = Uri.parse("content://sms"), MMS = Uri.parse("content://mms");
    private static final Uri OBSERVED = Uri.parse("content://mms-sms/");
    private static final int READ_CAP = 24 * 1024 * 1024;

    private static final String TAG = "QindaLink";
    private final Context ctx;
    private final ContentResolver cr;
    private final Map<Long, String> canonical = new HashMap<>();
    private ContentObserver observer;
    private volatile Runnable listener;

    public SmsProviderAndroid(Context ctx) {
        this.ctx = ctx.getApplicationContext();
        this.cr = this.ctx.getContentResolver();
    }

    @Override public boolean canRead() { return ctx.checkSelfPermission(Manifest.permission.READ_SMS) == PackageManager.PERMISSION_GRANTED; }
    @Override public boolean canSend() { return ctx.checkSelfPermission(Manifest.permission.SEND_SMS) == PackageManager.PERMISSION_GRANTED; }

    private static boolean after(long date, long id, long before, long beforeId) {
        return before < 0 || date < before || (date == before && id < beforeId);
    }

    private Map<Long, Integer> unreadByThread() {
        Map<Long, Integer> m = new HashMap<>();
        count(m, SMS, "read=0 AND type=1");
        count(m, MMS, "read=0 AND msg_box=1");
        return m;
    }

    private void count(Map<Long, Integer> m, Uri uri, String where) {
        try (Cursor c = cr.query(uri, new String[] {"thread_id"}, where, null, null)) {
            while (c != null && c.moveToNext()) m.merge(c.getLong(0), 1, Integer::sum);
        } catch (RuntimeException e) { warn("unread", e); }
    }

    /** Logs the operation and exception class only; never values. */
    private static void warn(String op, RuntimeException e) { Log.w(TAG, "sms provider " + op + ": " + e.getClass().getSimpleName()); }

    private static SmsProvider.ProviderException fail(String op, RuntimeException e) {
        warn(op, e);
        return new SmsProvider.ProviderException(op + " failed: " + e.getClass().getSimpleName());
    }

    private static final class Conv {
        long id, date; String recipients, snippet, address; int count = -1;
    }

    // AGENT-NOTE: capability probing. Verified on a Samsung SM-S948U (Android 17): ?simple=true accepts
    // _id,date,message_count,recipient_ids,snippet, while the plain conversations URI only accepts _id,date,read
    // (no such column: message_count/recipient_ids/snippet). AOSP behaves the other way round in places, so try
    // simple first, then the plain URI enriched from the newest message of each returned thread.
    private final FallbackChain<List<Conv>> convChain = new FallbackChain<>(java.util.Arrays.<Callable<List<Conv>>>asList(
            () -> queryConversations(CONVERSATIONS, new String[] {"_id", "date", "message_count", "recipient_ids", "snippet"}),
            () -> queryConversations(Uri.parse("content://mms-sms/conversations"), new String[] {"_id", "date"})));

    private List<Conv> queryConversations(Uri uri, String[] cols) {
        List<Conv> out = new ArrayList<>();
        try (Cursor c = cr.query(uri, cols, null, null, "date DESC")) {
            if (c == null) throw new IllegalStateException("null cursor");
            while (c.moveToNext()) {
                Conv v = new Conv();
                v.id = c.getLong(0); v.date = c.getLong(1);
                if (cols.length > 2) { v.count = c.getInt(2); v.recipients = c.getString(3); v.snippet = c.getString(4); }
                out.add(v);
            }
        }
        out.sort((a, b) -> a.date != b.date ? Long.compare(b.date, a.date) : Long.compare(b.id, a.id));
        return out;
    }

    private List<Conv> conversations() {
        try {
            return convChain.call("conversations");
        } catch (SmsProvider.ProviderException e) {
            Log.w(TAG, "sms provider " + e.getMessage());
            throw e;
        }
    }

    /** Fills snippet, address and count of a fallback row from its newest messages (counts and text stay in memory). */
    private void enrich(Conv v) {
        if (v.count >= 0) return;
        int n = 0;
        try (Cursor c = cr.query(SMS, new String[] {"address", "body"}, "thread_id=CAST(? AS INTEGER)",
                new String[] {Long.toString(v.id)}, "date DESC, _id DESC")) {
            if (c != null) { n += c.getCount(); if (c.moveToFirst()) { v.address = c.getString(0); v.snippet = c.getString(1); } }
        } catch (RuntimeException e) { warn("enrich-sms", e); }
        try (Cursor c = cr.query(MMS, new String[] {"_id"}, "thread_id=CAST(? AS INTEGER)", new String[] {Long.toString(v.id)}, null)) {
            if (c != null) n += c.getCount();
        } catch (RuntimeException e) { warn("enrich-mms", e); }
        v.count = n;
    }

    private List<String> addresses(String recipientIds) {
        List<String> out = new ArrayList<>();
        if (recipientIds == null) return out;
        for (String s : recipientIds.trim().split("\\s+")) {
            if (s.isEmpty()) continue;
            long id;
            try { id = Long.parseLong(s); } catch (NumberFormatException e) { continue; }
            String a = canonical.get(id);
            if (a == null) {
                try (Cursor c = cr.query(CANONICAL, new String[] {"address"}, "_id=?", new String[] {s}, null)) {
                    if (c != null && c.moveToFirst()) a = c.getString(0);
                } catch (RuntimeException e) { a = null; warn("canonical", e); }
                if (a != null) canonical.put(id, a);
            }
            if (a != null) out.add(a);
        }
        return out;
    }

    @Override public List<ThreadRow> threads(int limit, long before, long beforeId) {
        List<ThreadRow> out = new ArrayList<>();
        if (!canRead()) return out;
        Map<Long, Integer> unread = unreadByThread();
        for (Conv v : conversations()) {
            if (out.size() >= limit) break;
            if (!after(v.date, v.id, before, beforeId)) continue;
            enrich(v);
            List<String> addrs = v.recipients != null ? addresses(v.recipients) : new ArrayList<String>();
            if (addrs.isEmpty() && v.address != null) addrs.add(v.address);
            out.add(new ThreadRow(v.id, v.date, addrs, v.snippet, unread.getOrDefault(v.id, 0), v.count));
        }
        return out;
    }

    @Override public Map<Long, String> fingerprints() {
        Map<Long, String> out = new LinkedHashMap<>();
        if (!canRead()) return out;
        Map<Long, Integer> unread = unreadByThread();
        for (Conv v : conversations()) out.put(v.id, v.date + "/" + v.count + "/" + unread.getOrDefault(v.id, 0));
        return out;
    }

    @Override public int unreadTotal() {
        if (!canRead()) return 0;
        int n = 0;
        for (int v : unreadByThread().values()) n += v;
        return n;
    }

    private static String smsBox(int type) {
        switch (type) {
            case 1: return "inbox";
            case 2: return "sent";
            case 3: return "draft";
            case 4: return "outbox";
            case 5: return "failed";
            case 6: return "queued";
            default: return "inbox";
        }
    }

    private static String mmsBox(int box) {
        switch (box) {
            case 2: return "sent";
            case 3: return "draft";
            case 4: return "outbox";
            default: return "inbox";
        }
    }

    @Override public List<MessageRow> smsMessages(long thread, int limit, long before, long beforeId) {
        List<MessageRow> out = new ArrayList<>();
        if (!canRead()) return out;
        String[] q = SmsQueries.messageSelection(false, thread, before, beforeId);
        try (Cursor c = cr.query(SMS, new String[] {"_id", "address", "date", "body", "read", "type"}, q[0],
                java.util.Arrays.copyOfRange(q, 1, q.length), "date DESC, _id DESC")) {
            while (c != null && out.size() < limit && c.moveToNext()) {
                out.add(new MessageRow(c.getLong(0), false, smsBox(c.getInt(5)), c.getString(1), c.getLong(2), c.getString(3),
                        c.getInt(4) != 0, null));
            }
        } catch (RuntimeException e) { throw fail("sms-messages", e); }
        return out;
    }

    @Override public List<MessageRow> mmsMessages(long thread, int limit, long before, long beforeId) {
        List<MessageRow> out = new ArrayList<>();
        if (!canRead()) return out;
        String[] q = SmsQueries.messageSelection(true, thread, before, beforeId);
        try (Cursor c = cr.query(MMS, new String[] {"_id", "date", "msg_box", "read"}, q[0],
                java.util.Arrays.copyOfRange(q, 1, q.length), "date DESC, _id DESC")) {
            while (c != null && out.size() < limit && c.moveToNext()) {
                // Header only: the model hydrates the rows that make the page (parts and address cost queries).
                out.add(new MessageRow(c.getLong(0), true, mmsBox(c.getInt(2)), null, c.getLong(1) * 1000L, null, c.getInt(3) != 0, null));
            }
        } catch (RuntimeException e) { throw fail("mms-messages", e); }
        return out;
    }

    @Override public MessageRow hydrate(MessageRow h) {
        if (!h.mms) return h;
        List<PartRow> parts = new ArrayList<>();
        String body = readParts(h.id, parts);
        return new MessageRow(h.id, true, h.box, mmsAddress(h.id, "inbox".equals(h.box)), h.date, body, h.read, parts);
    }

    private String mmsAddress(long id, boolean inbox) {
        int want = inbox ? 137 : 151; // PduHeaders.FROM / TO
        try (Cursor c = cr.query(Uri.parse("content://mms/" + id + "/addr"), new String[] {"address", "type"}, "type=?",
                new String[] {String.valueOf(want)}, null)) {
            while (c != null && c.moveToNext()) {
                String a = c.getString(0);
                if (a != null && !a.startsWith("insert-address")) return a;
            }
        } catch (RuntimeException e) { warn("mms-addr", e); }
        return "";
    }

    /** Fills {@code parts} with non-text, non-SMIL parts and returns the folded text. */
    private String readParts(long mmsId, List<PartRow> parts) {
        StringBuilder text = new StringBuilder();
        try (Cursor c = cr.query(Uri.parse("content://mms/part"), new String[] {"_id", "ct", "name", "cl", "text"}, "mid=?",
                new String[] {String.valueOf(mmsId)}, null)) {
            while (c != null && c.moveToNext()) {
                long pid = c.getLong(0);
                String ct = c.getString(1) == null ? "" : c.getString(1);
                if (ct.equals("application/smil")) continue;
                if (ct.equals("text/plain")) {
                    String t = c.getString(4);
                    if (t != null) { if (text.length() > 0) text.append('\n'); text.append(t); }
                    continue;
                }
                String name = c.getString(2) != null ? c.getString(2) : c.getString(3);
                // size needs an fd per part; only the first MAX listed parts are described (protocol caps at 16)
                if (parts.size() < 16) parts.add(new PartRow(pid, ct, name, partSize(pid)));
            }
        } catch (RuntimeException e) { warn("mms-parts", e); }
        return text.toString();
    }

    private long partSize(long partId) {
        try (android.content.res.AssetFileDescriptor fd = cr.openAssetFileDescriptor(Uri.parse("content://mms/part/" + partId), "r")) {
            return fd == null ? -1 : fd.getLength();
        } catch (IOException | RuntimeException e) {
            return -1;
        }
    }

    @Override public PartData part(long partId, int maxEdge, int maxBytes) throws TooLarge {
        if (!canRead()) return null;
        Uri u = Uri.parse("content://mms/part/" + partId);
        String mime = null;
        try (Cursor c = cr.query(u, new String[] {"ct"}, null, null, null)) {
            if (c != null && c.moveToFirst()) mime = c.getString(0);
        } catch (RuntimeException e) { return null; }
        if (mime == null) return null;
        try {
            if (mime.startsWith("image/")) return image(u, mime, maxEdge, maxBytes);
            byte[] raw = readCapped(u, maxBytes + 1);
            if (raw == null) return null;
            if (raw.length > maxBytes) throw new TooLarge();
            return new PartData(mime, raw, false);
        } catch (IOException | RuntimeException e) {
            return null;
        }
    }

    private PartData image(Uri u, String mime, int maxEdge, int maxBytes) throws IOException, TooLarge {
        BitmapFactory.Options o = new BitmapFactory.Options();
        o.inJustDecodeBounds = true;
        try (InputStream in = cr.openInputStream(u)) { BitmapFactory.decodeStream(in, null, o); }
        if (o.outWidth <= 0) return null;
        if (Math.max(o.outWidth, o.outHeight) <= maxEdge) {
            byte[] raw = readCapped(u, maxBytes + 1);
            if (raw != null && raw.length <= maxBytes) return new PartData(mime, raw, false);
        }
        int sample = 1;
        while (Math.max(o.outWidth, o.outHeight) / (sample * 2) >= maxEdge) sample *= 2;
        BitmapFactory.Options d = new BitmapFactory.Options();
        d.inSampleSize = sample;
        Bitmap bmp;
        try (InputStream in = cr.openInputStream(u)) { bmp = BitmapFactory.decodeStream(in, null, d); }
        if (bmp == null) throw new TooLarge();
        int edge = maxEdge;
        for (int attempt = 0; attempt < 6; attempt++) {
            float k = Math.min(1f, (float) edge / Math.max(bmp.getWidth(), bmp.getHeight()));
            Bitmap s = k < 1f ? Bitmap.createScaledBitmap(bmp, Math.max(1, Math.round(bmp.getWidth() * k)), Math.max(1, Math.round(bmp.getHeight() * k)), true) : bmp;
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            s.compress(Bitmap.CompressFormat.JPEG, attempt < 3 ? 85 - attempt * 15 : 50, bo);
            if (bo.size() <= maxBytes) return new PartData("image/jpeg", bo.toByteArray(), true);
            edge = Math.max(64, edge * 3 / 4);
        }
        throw new TooLarge();
    }

    /** Reads up to {@code max} bytes; null when the part cannot be opened. */
    private byte[] readCapped(Uri u, int max) throws IOException {
        try (InputStream in = cr.openInputStream(u)) {
            if (in == null) return null;
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] buf = new byte[16384];
            int cap = Math.min(max, READ_CAP);
            for (int n; bo.size() < cap && (n = in.read(buf, 0, Math.min(buf.length, cap - bo.size()))) > 0; ) bo.write(buf, 0, n);
            return bo.toByteArray();
        }
    }

    @Override public boolean markRead(long thread) {
        if (!canRead()) return false;
        int pending = 0;
        Map<Long, Integer> unread = unreadByThread();
        pending = unread.getOrDefault(thread, 0);
        if (pending == 0) return true;
        int changed = 0;
        ContentValues v = new ContentValues();
        v.put("read", 1);
        v.put("seen", 1);
        try {
            changed += cr.update(SMS, v, "thread_id=? AND read=0", new String[] {String.valueOf(thread)});
            changed += cr.update(MMS, v, "thread_id=? AND read=0", new String[] {String.valueOf(thread)});
        } catch (SecurityException | UnsupportedOperationException e) {
            return false; // not the default SMS app
        }
        return changed > 0;
    }

    @Override public synchronized void setChangeListener(Runnable l) {
        listener = l;
        if (l != null && observer == null && canRead()) {
            observer = new ContentObserver(new Handler(Looper.getMainLooper())) {
                @Override public void onChange(boolean selfChange) {
                    Runnable r = listener;
                    if (r != null) r.run();
                }
            };
            cr.registerContentObserver(OBSERVED, true, observer);
        } else if (l == null && observer != null) {
            cr.unregisterContentObserver(observer);
            observer = null;
        }
    }
}
