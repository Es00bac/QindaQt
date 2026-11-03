package org.qindaqt.link.notify;

import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;
import org.qindaqt.link.proto.Envelope;

/**
 * Builds and checks `notify.*` frame bodies (docs/wiki/reference/qindalink-protocol-v1.md,
 * "Notifications"). Pure Java.
 *
 * AGENT-CONTRACT: the limits are the desktop's in
 * src/services/link_notifications/.../phone_notification.h and count UTF-16 code
 * units. Builders truncate (never reject) title and text; they cap the action list.
 */
public final class NotifyFrames {
    public static final int KEY = 512, PACKAGE = 256, APP = 128, TITLE = 256, TEXT = 2048;
    public static final int ACTIONS = 8, ACTION_ID = 64, ACTION_TITLE = 64, REPLY_TEXT = 4096;
    public static final int SNAPSHOT_ITEMS = 50, ICON_BASE64 = 65536;
    private static final Pattern HASH = Pattern.compile("[0-9a-f]{64}");

    private NotifyFrames() { }

    public static boolean isHash(String s) { return s != null && HASH.matcher(s).matches(); }

    /** Truncates to {@code max} UTF-16 units without splitting a surrogate pair. */
    public static String clip(String s, int max) {
        if (s == null) return "";
        if (s.length() <= max) return s;
        int end = max;
        if (end > 0 && Character.isHighSurrogate(s.charAt(end - 1))) end--;
        return s.substring(0, end);
    }

    /** The body of notify.posted (and of one snapshot item). */
    public static Map<String, Object> postedBody(PhoneNotification n) {
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("key", clip(n.key, KEY));
        b.put("pkg", clip(n.pkg, PACKAGE));
        b.put("app", clip(n.app.isEmpty() ? n.pkg : n.app, APP));
        b.put("title", clip(n.title, TITLE));
        b.put("text", clip(n.text, TEXT));
        b.put("when", Math.max(0L, n.when));
        b.put("icon", isHash(n.iconHash) ? n.iconHash : "");
        List<Object> acts = new ArrayList<>();
        for (PhoneNotification.Action a : n.actions) {
            if (acts.size() >= ACTIONS) break;
            if (a.id.isEmpty() || a.id.length() > ACTION_ID || a.title.isEmpty()) continue;
            Map<String, Object> m = new LinkedHashMap<>();
            m.put("id", a.id);
            m.put("title", clip(a.title, ACTION_TITLE));
            m.put("reply", a.reply);
            acts.add(m);
        }
        b.put("actions", acts);
        return b;
    }

    public static Envelope posted(PhoneNotification n) { return Envelope.of("notify.posted", postedBody(n)); }

    public static Envelope removed(String key) {
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("key", clip(key, KEY));
        return Envelope.of("notify.removed", b);
    }

    /** At most {@link #SNAPSHOT_ITEMS} items; the caller orders them (newest first). */
    public static Envelope snapshot(List<PhoneNotification> items) {
        List<Object> list = new ArrayList<>();
        for (PhoneNotification n : items) {
            if (list.size() >= SNAPSHOT_ITEMS) break;
            list.add(postedBody(n));
        }
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("items", list);
        return Envelope.of("notify.snapshot", b);
    }

    /** Reply to a desktop icon request; {@code png} null or oversized means "not available" (empty). */
    public static Envelope icon(String hash, byte[] png) {
        String enc = png == null ? "" : Base64.getEncoder().encodeToString(png);
        if (enc.length() > ICON_BASE64) enc = "";
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("hash", hash);
        b.put("png", enc);
        return Envelope.of("notify.icon", b);
    }

    /** A string field with 1..max units, or null when absent, mistyped or out of bounds. */
    public static String field(Map<String, Object> b, String name, int max) {
        Object v = b.get(name);
        if (!(v instanceof String)) return null;
        String s = (String) v;
        return s.isEmpty() || s.length() > max ? null : s;
    }

    /** Optional reply text of notify.action: null when absent, "" allowed; {@code ok[0]} false when invalid. */
    public static String replyText(Map<String, Object> b, boolean[] ok) {
        ok[0] = true;
        if (!b.containsKey("text")) return null;
        Object v = b.get("text");
        if (!(v instanceof String) || ((String) v).length() > REPLY_TEXT) { ok[0] = false; return null; }
        return (String) v;
    }
}
