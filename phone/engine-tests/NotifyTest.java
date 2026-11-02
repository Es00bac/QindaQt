import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.notify.NotificationFeature;
import org.qindaqt.link.notify.NotificationFilter;
import org.qindaqt.link.notify.NotificationSource;
import org.qindaqt.link.notify.NotifyFrames;
import org.qindaqt.link.notify.PhoneNotification;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.Json;

/** JVM tests of the phone notify feature: frames, bounds, filtering, coalescing, dismiss/action/icon. */
public final class NotifyTest {
    static int failures;

    static void check(boolean ok, String what) {
        if (!ok) { failures++; System.out.println("FAIL " + what); } else System.out.println("ok   " + what);
    }

    static final class Ctx implements FeatureContext {
        final List<Envelope> sent = new ArrayList<>();
        final List<String> replies = new ArrayList<>();
        boolean peer = true;
        int wakes;
        public boolean send(Envelope e) { if (!peer) return false; sent.add(e); return true; }
        public long nextId() { return 1; }
        public boolean peerHas(String c) { return peer; }
        public void replyOk(long id) { replies.add(id + ":ok"); }
        public void replyError(long id, String code, String m) { replies.add(id + ":" + code); }
        public void wake() { wakes++; }
    }

    static final class Src implements NotificationSource {
        Listener l;
        boolean access = true;
        List<PhoneNotification> active = new ArrayList<>();
        List<String> dismissed = new ArrayList<>();
        List<String> performed = new ArrayList<>();
        String performResult;
        public void setListener(Listener x) { l = x; }
        public boolean accessGranted() { return access; }
        public List<PhoneNotification> active() { return active; }
        public boolean dismiss(String key) { dismissed.add(key); return key.equals("live"); }
        public String perform(String key, String action, String title, String text) { performed.add(key + "/" + action + "/" + title + "/" + text); return performResult; }
        public byte[] icon(String hash) { return hash.startsWith("aa") ? new byte[] {1, 2, 3} : null; }
    }

    static final String H = "ab".repeat(32);

    static PhoneNotification n(String key, String title, String text, long when) {
        return new PhoneNotification(key, "com.chat", "Chat", title, text, when, H,
                Arrays.asList(new PhoneNotification.Action("0", "Archive", false), new PhoneNotification.Action("1", "Reply", true)));
    }

    static Envelope req(String t, long id, Object... kv) {
        Map<String, Object> b = new java.util.LinkedHashMap<>();
        for (int i = 0; i < kv.length; i += 2) b.put((String) kv[i], kv[i + 1]);
        return new Envelope(t, id, 0, b);
    }

    @SuppressWarnings("unchecked")
    public static void main(String[] a) throws Exception {
        // --- filter
        check(NotificationFilter.mirror(false, false, false, "com.chat", "org.qindaqt.link"), "filter: normal notification mirrored");
        check(!NotificationFilter.mirror(true, false, false, "com.chat", "org.qindaqt.link"), "filter: ongoing skipped");
        check(!NotificationFilter.mirror(false, true, false, "com.chat", "org.qindaqt.link"), "filter: foreground service skipped");
        check(!NotificationFilter.mirror(false, false, true, "com.chat", "org.qindaqt.link"), "filter: group summary skipped");
        check(!NotificationFilter.mirror(false, false, false, "org.qindaqt.link", "org.qindaqt.link"), "filter: own notifications skipped");

        // --- frame building and bounds
        String longText = "x".repeat(5000);
        String emoji = "a".repeat(255) + "😀";  // surrogate pair straddles the 256 limit
        PhoneNotification big = new PhoneNotification("k".repeat(600), "p".repeat(300), "", emoji, longText, -5, "nothex",
                Collections.nCopies(12, new PhoneNotification.Action("i", "t".repeat(100), false)));
        Map<String, Object> b = NotifyFrames.postedBody(big);
        check(((String) b.get("key")).length() == 512 && ((String) b.get("pkg")).length() == 256, "bounds: key and package clipped");
        check(((String) b.get("text")).length() == 2048, "bounds: text clipped to 2048");
        check(((String) b.get("title")).length() == 255, "bounds: clip never splits a surrogate pair");
        check(((String) b.get("app")).equals(((String) b.get("pkg"))) || ((String) b.get("app")).length() <= 128, "bounds: empty label falls back to package, clipped to 128");
        check(((Long) b.get("when")) == 0L && b.get("icon").equals(""), "bounds: negative time and bad icon hash normalised");
        check(((List<Object>) b.get("actions")).size() == 8, "bounds: at most 8 actions");
        check(((String) ((Map<String, Object>) ((List<Object>) b.get("actions")).get(0)).get("title")).length() == 64, "bounds: action title clipped");
        List<PhoneNotification> many = new ArrayList<>();
        for (int i = 0; i < 80; i++) many.add(n("k" + i, "t", "x", i));
        check(((List<Object>) NotifyFrames.snapshot(many).b.get("items")).size() == 50, "bounds: snapshot holds at most 50 items");
        Envelope round = NotifyFrames.posted(n("k1", "T", "X", 7));
        check(round.t.equals("notify.posted") && round.b.get("icon").equals(H), "posted frame carries type and icon hash");
        check(NotifyFrames.icon(H, new byte[70000]).b.get("png").equals(""), "icon: oversized PNG becomes unavailable");
        check(Json.write(NotifyFrames.removed("k").b).contains("\"key\":\"k\""), "removed frame body");

        // --- ready: snapshot, newest first, access state
        Src src = new Src();
        src.active = new ArrayList<>(Arrays.asList(n("old", "T", "x", 1), n("new", "T", "x", 9)));
        EngineState st = new EngineState();
        NotificationFeature f = new NotificationFeature(src, st);
        Ctx c = new Ctx();
        f.onReady(c);
        check(c.sent.size() == 1 && c.sent.get(0).t.equals("notify.snapshot"), "ready: snapshot sent once");
        List<Object> items = (List<Object>) c.sent.get(0).b.get("items");
        check(((Map<String, Object>) items.get(0)).get("key").equals("new"), "ready: snapshot newest first");
        check(st.toJson().contains("\"notifyAccess\":\"granted\""), "state: access granted reported");
        Src noAccess = new Src();
        noAccess.access = false;
        EngineState st2 = new EngineState();
        new NotificationFeature(noAccess, st2).onReady(new Ctx());
        check(st2.toJson().contains("\"notifyAccess\":\"missing\""), "state: missing access reported");

        // --- coalescing, dedupe, removal
        c.sent.clear();
        src.l.onPosted(n("k1", "A", "1", 5));
        src.l.onPosted(n("k1", "A", "2", 5));
        src.l.onPosted(n("k2", "B", "1", 6));
        f.onTick(0);
        check(c.sent.size() == 2 && c.sent.get(0).b.get("text").equals("2"), "tick: updates to one key coalesce to the latest");
        c.sent.clear();
        src.l.onPosted(n("k1", "A", "2", 5));
        f.onTick(0);
        check(c.sent.isEmpty(), "tick: identical repost is not resent");
        src.l.onRemoved("k1");
        f.onTick(0);
        check(c.sent.size() == 1 && c.sent.get(0).t.equals("notify.removed"), "tick: removal sent");
        c.sent.clear();
        for (int i = 0; i < 45; i++) src.l.onPosted(n("b" + i, "T", "x", i));
        f.onTick(0);
        check(c.sent.size() == 20, "rate: at most 20 frames per tick");
        int w = c.wakes;
        f.onTick(500); f.onTick(1000);
        check(c.sent.size() == 45, "rate: the rest follows on later ticks");
        // access regained: snapshot again
        c.sent.clear();
        src.l.onAccessChanged(true);
        f.onTick(0);
        check(c.sent.size() == 1 && c.sent.get(0).t.equals("notify.snapshot"), "listener reconnect re-sends a snapshot");

        // --- a mirrored notification that becomes ongoing is retracted; unknown keys stay silent
        c.sent.clear();
        src.l.onPosted(n("flip", "A", "1", 5));
        f.onTick(0);
        c.sent.clear();
        src.l.onRemoved("flip");   // what the Android source does for a post that is now filtered
        src.l.onRemoved("never-sent");
        f.onTick(0);
        check(c.sent.size() == 1 && c.sent.get(0).t.equals("notify.removed") && c.sent.get(0).b.get("key").equals("flip"), "became ongoing: sent copy retracted, never-sent key silent");

        // --- desktop requests
        c.sent.clear();
        check(f.onFrame(req("notify.dismiss", 3, "key", "live")) && c.replies.contains("3:ok") && src.dismissed.contains("live"), "dismiss: cancels and replies ok");
        f.onFrame(req("notify.dismiss", 4, "key", "gone"));
        check(c.replies.contains("4:target-changed"), "dismiss: unknown key is target-changed");
        f.onFrame(req("notify.dismiss", 5));
        check(c.replies.contains("5:bad-request"), "dismiss: missing key is bad-request");
        f.onFrame(req("notify.dismiss", 0, "key", "live"));
        check(c.replies.size() == 3, "dismiss without id sends no reply");
        f.onFrame(req("notify.action", 6, "key", "k1", "action", "1", "title", "Reply", "text", "hi there"));
        check(src.performed.contains("k1/1/Reply/hi there") && c.replies.contains("6:ok"), "action: reply text reaches the source");
        f.onFrame(req("notify.action", 7, "key", "k1", "action", "0", "title", "Archive"));
        check(src.performed.contains("k1/0/Archive/null"), "action: no text for a plain action");
        src.performResult = "target-changed";
        f.onFrame(req("notify.action", 8, "key", "k1", "action", "0", "title", "Archive"));
        check(c.replies.contains("8:target-changed"), "action: source error code is returned");
        src.performResult = null;
        f.onFrame(req("notify.action", 9, "key", "k1", "action", "1", "title", "Reply", "text", "z".repeat(4097)));
        check(c.replies.contains("9:bad-request") && src.performed.size() == 3, "action: reply text over 4096 refused");
        f.onFrame(req("notify.action", 10, "key", "k1", "action", "1", "title", "Reply", "text", 5L));
        check(c.replies.contains("10:bad-request"), "action: non-string text refused");
        f.onFrame(req("notify.action", 20, "key", "k1", "action", "0"));
        check(c.replies.contains("20:bad-request"), "action: title is required");
        src.performResult = "unverified";
        f.onFrame(req("notify.action", 21, "key", "k1", "action", "0", "title", "Archive"));
        check(c.replies.contains("21:unsupported"), "action: activity launch is answered unsupported/unverified, not ok");
        src.performResult = null;
        src.access = false;
        f.onFrame(req("notify.action", 11, "key", "k1", "action", "0", "title", "Archive"));
        check(c.replies.contains("11:denied"), "action: no notification access is denied");
        src.access = true;
        f.onFrame(req("notify.icon", 0, "hash", "aa".repeat(32)));
        Envelope ic = c.sent.get(c.sent.size() - 1);
        check(ic.t.equals("notify.icon") && ic.b.get("png").equals("AQID"), "icon: PNG returned base64");
        f.onFrame(req("notify.icon", 0, "hash", "bb".repeat(32)));
        check(c.sent.get(c.sent.size() - 1).b.get("png").equals(""), "icon: unknown hash answered empty");
        check(!f.onFrame(req("notify.posted", 12)), "posted from the desktop is not handled");

        // --- peer without notify capability: nothing is sent
        Ctx quiet = new Ctx();
        quiet.peer = false;
        NotificationFeature f3 = new NotificationFeature(src, new EngineState());
        f3.onReady(quiet);
        src.l.onPosted(n("k9", "A", "1", 5));
        f3.onTick(0);
        check(quiet.sent.isEmpty(), "no frames when the desktop lacks the capability");
        f3.onClosed();
        check(src.l == null, "closed: listener unregistered");

        System.out.println(failures == 0 ? "NotifyTest OK" : "NotifyTest FAILED " + failures);
        System.exit(failures == 0 ? 0 : 1);
    }
}
