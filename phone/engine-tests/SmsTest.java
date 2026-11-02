import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.sms.ContactResolver;
import org.qindaqt.link.sms.SendTracker;
import org.qindaqt.link.sms.SmsFeature;
import org.qindaqt.link.sms.SmsProvider;
import org.qindaqt.link.sms.SmsSender;

/** Pure sms model, paging, bounds, change events, send mapping over a fake provider (no Android, no real data). */
public final class SmsTest {
    static void expect(boolean ok, String what) { SessionTest.expect(ok, what); }

    static final class FakeProvider implements SmsProvider {
        final List<ThreadRow> threads = new ArrayList<>();
        final List<MessageRow> sms = new ArrayList<>(), mms = new ArrayList<>();
        boolean read = true, send = true, markOk = false;
        int unread = 7;
        Runnable listener;
        Map<Long, String> fp = new LinkedHashMap<>();
        static <T> List<T> page(List<T> all, int limit, long before, long beforeId, java.util.function.ToLongFunction<T> date, java.util.function.ToLongFunction<T> id) {
            List<T> sorted = new ArrayList<>(all);
            sorted.sort((a, b) -> date.applyAsLong(a) != date.applyAsLong(b) ? Long.compare(date.applyAsLong(b), date.applyAsLong(a)) : Long.compare(id.applyAsLong(b), id.applyAsLong(a)));
            List<T> out = new ArrayList<>();
            for (T t : sorted) {
                if (before >= 0 && !(date.applyAsLong(t) < before || (date.applyAsLong(t) == before && id.applyAsLong(t) < beforeId))) continue;
                if (out.size() >= limit) break;
                out.add(t);
            }
            return out;
        }
        public boolean canRead() { return read; }
        public boolean canSend() { return send; }
        public List<ThreadRow> threads(int l, long b, long bi) { return page(threads, l, b, bi, t -> t.date, t -> t.id); }
        public List<MessageRow> smsMessages(long th, int l, long b, long bi) { return page(sms, l, b, bi, t -> t.date, t -> t.id); }
        public List<MessageRow> mmsMessages(long th, int l, long b, long bi) { return page(mms, l, b, bi, t -> t.date, t -> t.id); }
        public Map<Long, String> fingerprints() { return new LinkedHashMap<>(fp); }
        public int unreadTotal() { return unread; }
        public PartData part(long id, int edge, int max) throws TooLarge {
            if (id == 1) return new PartData("image/jpeg", new byte[] {1, 2, 3}, true);
            if (id == 2) throw new TooLarge();
            return null;
        }
        public boolean markRead(long t) { return markOk; }
        public void setChangeListener(Runnable r) { listener = r; }
    }

    static final class Ctx implements FeatureContext {
        final List<Envelope> sent = new ArrayList<>();
        int wakes;
        public boolean send(Envelope e) { sent.add(e); return true; }
        public long nextId() { return 1; }
        public boolean peerHas(String c) { return true; }
        public void replyOk(long id) { sent.add(new Envelope("ok", 0, id, null)); }
        public void replyError(long id, String code, String msg) {
            Map<String, Object> b = new LinkedHashMap<>(); b.put("code", code);
            sent.add(new Envelope("error", 0, id, b));
        }
        public void wake() { wakes++; }
        Envelope last() { return sent.get(sent.size() - 1); }
    }

    static final class FakeSender implements SmsSender {
        Listener l; long ref; List<String> addrs; String text; int sub;
        public void send(long r, List<String> a, String t, int s, Listener li) {
            if (t.equals("explode")) throw new IllegalArgumentException("x");
            l = li; ref = r; addrs = a; text = t; sub = s;
        }
    }

    static Envelope req(String t, long id, Object... kv) { return new Envelope(t, id, 0, SessionTest.m(kv)); }

    @SuppressWarnings("unchecked")
    public static void main(String[] a) throws Exception {
        FakeProvider p = new FakeProvider();
        for (int i = 1; i <= 130; i++) p.threads.add(new SmsProvider.ThreadRow(i, 1000 + i, Arrays.asList("+100" + i), "snip" + i, i % 3, i));
        for (int i = 1; i <= 250; i++) p.sms.add(new SmsProvider.MessageRow(i, false, "inbox", "+1", 5000 + i * 2, "b" + i, true, null));
        for (int i = 1; i <= 10; i++) p.mms.add(new SmsProvider.MessageRow(i, true, "sent", "+2", 5001 + i * 40, "m", true,
                Arrays.asList(new SmsProvider.PartRow(1, "image/jpeg", "n.jpg", 3))));
        ContactResolver names = addr -> addr.equals("+1001") ? "Alice" : "";
        FakeSender sender = new FakeSender();
        EngineState st = new EngineState();
        SmsFeature f = new SmsFeature(p, names, sender, st);
        Ctx c = new Ctx();
        f.onReady(c);
        expect(c.sent.size() == 1 && c.last().t.equals("sms.changed") && Long.valueOf(7).equals(c.last().b.get("unread")), "ready announces unread total");
        expect(st.toJson().contains("\"sms\":\"read,send\""), "engine state carries the permission summary only");

        // Threads: clamp to 100, default 50, paging via cursor, names resolved.
        expect(f.onFrame(req("sms.threads", 1, "limit", 5000L)), "threads handled");
        Map<String, Object> r = c.last().b;
        expect(c.last().re == 1 && ((List<?>) r.get("threads")).size() == 100 && Boolean.TRUE.equals(r.get("more")), "limit clamped to 100 with more=true");
        f.onFrame(req("sms.threads", 2));
        expect(((List<?>) c.last().b.get("threads")).size() == 50, "default limit is 50");
        List<Map<String, Object>> th = (List<Map<String, Object>>) c.last().b.get("threads");
        expect(th.get(0).get("id").equals(130L), "newest first");
        Map<String, Object> last = th.get(th.size() - 1);
        f.onFrame(req("sms.threads", 3, "limit", 100L, "before", last.get("date"), "beforeId", last.get("id")));
        List<Map<String, Object>> th2 = (List<Map<String, Object>>) c.last().b.get("threads");
        expect(th2.size() == 80 && th2.get(0).get("id").equals(80L) && Boolean.FALSE.equals(c.last().b.get("more")), "second page continues after the cursor and ends");
        f.onFrame(req("sms.threads", 4, "limit", 200L, "before", 1002L, "beforeId", 2L));
        th2 = (List<Map<String, Object>>) c.last().b.get("threads");
        expect(th2.size() == 1 && th2.get(0).get("names").equals(Arrays.asList("Alice")), "contact name resolved");
        p.threads.add(new SmsProvider.ThreadRow(999, 5, Arrays.asList("+1001"), null, -4, -2));
        f.onFrame(req("sms.threads", 5, "limit", 1L, "before", 6L, "beforeId", 0L));
        Map<String, Object> t0 = ((List<Map<String, Object>>) c.last().b.get("threads")).get(0);
        expect(t0.get("names").equals(Arrays.asList("Alice")) && t0.get("snippet").equals("") && t0.get("unread").equals(0L) && t0.get("count").equals(0L), "contact resolved; null/negative sanitized");
        f.onFrame(req("sms.threads", 6, "limit", "x"));
        expect(c.last().t.equals("error") && "bad-request".equals(c.last().b.get("code")), "wrong field type -> bad-request");

        // Messages: merge sms+mms newest first, clamp 200, kinds, parts.
        f.onFrame(req("sms.messages", 7, "thread", 1L, "limit", 9999L));
        List<Map<String, Object>> ms = (List<Map<String, Object>>) c.last().b.get("messages");
        expect(ms.size() == 200 && Boolean.TRUE.equals(c.last().b.get("more")), "messages clamped to 200");
        boolean sorted = true;
        for (int i = 1; i < ms.size(); i++) if ((Long) ms.get(i - 1).get("date") < (Long) ms.get(i).get("date")) sorted = false;
        expect(sorted, "merged sms+mms are newest first");
        boolean sawMms = false;
        for (Map<String, Object> m : ms) if ("mms".equals(m.get("kind"))) { sawMms = true; expect(((List<?>) m.get("parts")).size() == 1, "mms part descriptor present"); }
        expect(sawMms, "mms interleaved");
        Map<String, Object> lm = ms.get(ms.size() - 1);
        f.onFrame(req("sms.messages", 8, "thread", 1L, "limit", 200L, "before", lm.get("date"), "beforeId", lm.get("id")));
        expect(c.last().b.get("messages") != null && ((List<?>) c.last().b.get("messages")).size() == 60 && Boolean.FALSE.equals(c.last().b.get("more")), "paged to the end: 260 total");
        f.onFrame(req("sms.messages", 9));
        expect("bad-request".equals(c.last().b.get("code")), "thread required");

        // Body cap, parts cap, reply budget.
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 5000; i++) big.append('\u0001'); // escapes to 6 bytes each
        FakeProvider q = new FakeProvider();
        List<SmsProvider.PartRow> many = new ArrayList<>();
        for (int i = 0; i < 40; i++) many.add(new SmsProvider.PartRow(i, "a/b", "n", 1));
        for (int i = 1; i <= 200; i++) q.sms.add(new SmsProvider.MessageRow(i, false, "inbox", "+1", i, big + "", true, i == 200 ? many : null));
        SmsFeature f2 = new SmsFeature(q, names, sender, new EngineState());
        Ctx c2 = new Ctx();
        f2.onReady(c2);
        f2.onFrame(req("sms.messages", 1, "thread", 1L, "limit", 200L));
        List<Map<String, Object>> ms2 = (List<Map<String, Object>>) c2.last().b.get("messages");
        expect(((String) ms2.get(0).get("body")).length() == 4000 && Boolean.TRUE.equals(ms2.get(0).get("trunc")), "body capped at 4000 chars with trunc flag");
        expect(ms2.size() < 200 && Boolean.TRUE.equals(c2.last().b.get("more")), "reply budget ends the page early: " + ms2.size());
        expect(org.qindaqt.link.proto.Json.write(c2.last().b).length() < 3 * 1024 * 1024, "reply under 3 MiB");
        boolean partsCapped = false;
        for (Map<String, Object> m : ms2) if (m.get("id").equals(200L)) partsCapped = ((List<?>) m.get("parts")).size() == 16;
        expect(partsCapped, "parts capped at 16");

        // Parts.
        f.onFrame(req("sms.part", 10, "part", 1L));
        expect(c.last().t.equals("sms.part") && "AQID".equals(c.last().b.get("data")) && Boolean.TRUE.equals(c.last().b.get("scaled")), "part base64");
        f.onFrame(req("sms.part", 11, "part", 2L));
        expect("unsupported".equals(c.last().b.get("code")), "oversized non-image is unsupported");
        f.onFrame(req("sms.part", 12, "part", 3L));
        expect("bad-request".equals(c.last().b.get("code")), "missing part");

        // Mark read: unsupported unless the provider accepts it.
        f.onFrame(req("sms.read", 13, "thread", 4L));
        expect("unsupported".equals(c.last().b.get("code")), "mark read unsupported for non-default app");
        p.markOk = true; p.unread = 3;
        f.onFrame(req("sms.read", 14, "thread", 4L));
        expect(c.last().t.equals("sms.read") && Long.valueOf(3).equals(c.last().b.get("unread")), "mark read ok reports new total");

        // Send via fake sender; events map to sms.sent.
        f.onFrame(req("sms.send", 15, "addrs", new ArrayList<Object>(Arrays.asList(" +15550001 ")), "text", "hello"));
        expect(c.last().t.equals("sms.send") && Long.valueOf(1).equals(c.last().b.get("ref")), "send accepted with a ref");
        expect(sender.addrs.equals(Arrays.asList("+15550001")) && sender.text.equals("hello") && sender.sub == -1, "sender got trimmed input");
        int before = c.sent.size();
        sender.l.onEvent(1, "sent", null);
        sender.l.onEvent(1, "failed", "no-service");
        f.onTick(System.currentTimeMillis());
        expect(c.sent.size() == before + 2 && c.sent.get(before).t.equals("sms.sent") && "sent".equals(c.sent.get(before).b.get("state"))
                && "no-service".equals(c.last().b.get("code")), "sent/failed events forwarded");
        for (Object[] bad : new Object[][] {{new ArrayList<Object>(), "x"}, {new ArrayList<Object>(Arrays.asList("1")), ""},
                {new ArrayList<Object>(Arrays.asList(7L)), "x"}, {new ArrayList<Object>(Arrays.asList("1")), "explode"}}) {
            f.onFrame(req("sms.send", 16, "addrs", bad[0], "text", bad[1]));
            expect("bad-request".equals(c.last().b.get("code")), "invalid send rejected");
        }
        StringBuilder longText = new StringBuilder();
        for (int i = 0; i < 1601; i++) longText.append('a');
        f.onFrame(req("sms.send", 17, "addrs", new ArrayList<Object>(Arrays.asList("1")), "text", longText.toString()));
        expect("bad-request".equals(c.last().b.get("code")), "text over 1600 chars rejected");
        p.send = false;
        f.onFrame(req("sms.send", 18, "addrs", new ArrayList<Object>(Arrays.asList("1")), "text", "x"));
        expect("denied".equals(c.last().b.get("code")), "send without permission is denied");
        p.send = true;

        // Changes: debounce, diff, unread.
        p.fp.put(1L, "a"); p.fp.put(2L, "a");
        f.onClosed(); f.onReady(c);
        int n = c.sent.size();
        p.fp.put(2L, "b"); p.fp.put(3L, "a"); p.unread = 9;
        p.listener.run();
        p.listener.run();
        f.onTick(System.currentTimeMillis());
        expect(c.sent.size() == n, "no change event before the debounce elapses");
        Thread.sleep(600);
        f.onTick(System.currentTimeMillis());
        expect(c.sent.size() == n + 1, "one coalesced change event");
        Map<String, Object> ch = c.last().b;
        List<Object> ids = (List<Object>) ch.get("threads");
        expect(c.last().t.equals("sms.changed") && ids.size() == 2 && ids.contains(2L) && ids.contains(3L) && Long.valueOf(9).equals(ch.get("unread")) && Boolean.FALSE.equals(ch.get("all")), "changed ids and unread");
        p.fp.clear();
        for (long i = 1; i <= 150; i++) p.fp.put(i, "z");
        p.listener.run();
        Thread.sleep(600);
        f.onTick(System.currentTimeMillis());
        expect(((List<?>) c.last().b.get("threads")).size() == 100 && Boolean.TRUE.equals(c.last().b.get("all")), "more than 100 changes -> all:true");

        // No permission: everything but send is denied; no provider reads.
        p.read = false;
        f.onFrame(req("sms.threads", 20));
        expect("denied".equals(c.last().b.get("code")), "no READ_SMS -> denied");
        expect(!f.onFrame(new Envelope("sms.bogus", 21, 0, null)), "unknown sms frame left to the session (unknown-type)");
        f.onClosed();
        expect(p.listener == null, "listener cleared on close");

        // SendTracker mapping.
        List<String> ev = new ArrayList<>();
        SendTracker tr = new SendTracker(5, 3, (ref, state, code) -> ev.add(ref + state + code));
        tr.partSent(null); tr.partSent(null);
        expect(ev.isEmpty(), "no sent before every part left");
        tr.partSent(null);
        expect(ev.equals(Arrays.asList("5sentnull")), "sent once after all parts");
        tr.partDelivered(true); tr.partDelivered(true); tr.partDelivered(true);
        expect(ev.size() == 2 && ev.get(1).equals("5deliverednull"), "delivered after all reports");
        ev.clear();
        tr = new SendTracker(6, 2, (ref, state, code) -> ev.add(ref + state + code));
        tr.partSent("radio-off"); tr.partSent(null); tr.partSent(null); tr.partDelivered(true);
        expect(ev.equals(Arrays.asList("6failedradio-off")), "failure is terminal");
        System.out.println("SmsTest OK");
    }
}
