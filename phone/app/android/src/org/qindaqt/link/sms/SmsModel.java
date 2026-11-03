package org.qindaqt.link.sms;

import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.proto.Json;

/**
 * Pure paging, bounding and shaping of provider rows into sms.* reply bodies (protocol page, Messages).
 *
 * AGENT-GUARD: every bound here is part of the wire contract (limits 100/200, string caps, 2 MiB reply budget).
 * The reply must stay far below the 4 MiB frame cap even for hostile data, so the budget counts the encoded size
 * of each row as it is added. Nothing in this class logs.
 */
public final class SmsModel {
    public static final int MAX_THREADS = 100, MAX_MESSAGES = 200, DEFAULT_LIMIT = 50;
    public static final int BUDGET_BYTES = 2 * 1024 * 1024;
    static final int SNIPPET = 200, BODY = 4000, ADDR = 64, NAME = 128, MIME = 128, MAX_ADDRS = 20, MAX_PARTS = 16;

    private final SmsProvider provider;
    private final ContactResolver contacts;

    public SmsModel(SmsProvider provider, ContactResolver contacts) {
        this.provider = provider;
        this.contacts = contacts;
    }

    /** Clamps a requested limit to 1..max, defaulting when absent (-1). */
    public static int clamp(long limit, int max) {
        if (limit < 0) return Math.min(DEFAULT_LIMIT, max);
        return (int) Math.max(1, Math.min(limit, max));
    }

    public Map<String, Object> threads(int limit, long before, long beforeId) {
        List<SmsProvider.ThreadRow> rows = provider.threads(limit + 1, before, beforeId);
        List<Object> out = new ArrayList<>();
        boolean more = false;
        int budget = 0;
        for (int i = 0; i < rows.size(); i++) {
            if (i >= limit) { more = true; break; }
            Map<String, Object> t = thread(rows.get(i));
            budget += Json.write(t).length();
            if (budget > BUDGET_BYTES && !out.isEmpty()) { more = true; break; }
            out.add(t);
        }
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("threads", out);
        b.put("more", more);
        b.put("unread", (long) provider.unreadTotal());
        return b;
    }

    private Map<String, Object> thread(SmsProvider.ThreadRow r) {
        List<Object> addrs = new ArrayList<>(), names = new ArrayList<>();
        for (String a : r.addrs) {
            if (addrs.size() >= MAX_ADDRS) break;
            String addr = cut(a, ADDR);
            addrs.add(addr);
            names.add(cut(contacts.name(addr), NAME));
        }
        Map<String, Object> t = new LinkedHashMap<>();
        t.put("id", r.id);
        t.put("addrs", addrs);
        t.put("names", names);
        t.put("snippet", cut(r.snippet, SNIPPET));
        t.put("date", r.date);
        t.put("unread", (long) Math.max(0, r.unread));
        t.put("count", (long) Math.max(0, r.count));
        return t;
    }

    /**
     * Messages are ordered by (date desc, kind desc with mms before sms, id desc) and the cursor is
     * (before, beforeMms, beforeId), so rows that share a millisecond are neither repeated nor skipped.
     * Each table is queried with the (date, id) cursor that is equivalent for its kind: after an mms cursor
     * row, same-date sms rows of any id still follow; after an sms cursor row, same-date mms rows are done.
     */
    public Map<String, Object> messages(long thread, int limit, long before, boolean beforeMms, long beforeId) {
        long smsId = beforeId, mmsId = beforeId;
        if (before >= 0) {
            if (beforeMms) smsId = Long.MAX_VALUE; else mmsId = -1;
        }
        List<SmsProvider.MessageRow> merged = new ArrayList<>(provider.smsMessages(thread, limit + 1, before, smsId));
        merged.addAll(provider.mmsMessages(thread, limit + 1, before, mmsId));
        merged.sort((x, y) -> x.date != y.date ? Long.compare(y.date, x.date)
                : x.mms != y.mms ? (x.mms ? -1 : 1) : Long.compare(y.id, x.id));
        List<Object> out = new ArrayList<>();
        boolean more = false;
        int budget = 0;
        for (int i = 0; i < merged.size(); i++) {
            if (i >= limit) { more = true; break; }
            Map<String, Object> m = message(provider.hydrate(merged.get(i)));
            budget += Json.write(m).length();
            if (budget > BUDGET_BYTES && !out.isEmpty()) { more = true; break; }
            out.add(m);
        }
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("thread", thread);
        b.put("messages", out);
        b.put("more", more);
        return b;
    }

    private Map<String, Object> message(SmsProvider.MessageRow r) {
        String body = r.body == null ? "" : r.body;
        boolean trunc = body.length() > BODY;
        List<Object> parts = new ArrayList<>();
        if (r.parts != null) {
            for (SmsProvider.PartRow p : r.parts) {
                if (parts.size() >= MAX_PARTS) break;
                Map<String, Object> pm = new LinkedHashMap<>();
                pm.put("id", p.id);
                pm.put("mime", cut(p.mime, MIME));
                pm.put("name", cut(p.name, NAME));
                pm.put("size", p.size < 0 ? -1L : p.size);
                parts.add(pm);
            }
        }
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("id", r.id);
        m.put("kind", r.mms ? "mms" : "sms");
        m.put("box", r.box);
        m.put("addr", cut(r.addr, ADDR));
        m.put("date", r.date);
        m.put("body", cut(body, BODY));
        m.put("trunc", trunc);
        m.put("read", r.read);
        m.put("parts", parts);
        return m;
    }

    /** Truncates to {@code max} chars without splitting a surrogate pair; null becomes "". */
    static String cut(String s, int max) {
        if (s == null) return "";
        if (s.length() <= max) return s;
        int end = max;
        if (Character.isHighSurrogate(s.charAt(end - 1))) end--;
        return s.substring(0, end);
    }
}
