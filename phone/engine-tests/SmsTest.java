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

    static class FakeProvider implements SmsProvider {
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

    static class FakeSender implements SmsSender {
        Listener l; long ref; List<String> addrs; String text; int sub;
        public void send(long r, List<String> a, String t, int s, Listener li) {
            if (t.equals("explode")) throw new IllegalArgumentException("x");
            l = li; ref = r; addrs = a; text = t; sub = s;
        }
    }

    static Envelope req(String t, long id, Object... kv) { return new Envelope(t, id, 0, SessionTest.m(kv)); }

    /** Runs the real selection clauses through sqlite3 with TEXT-bound values, like ContentResolver. */
    static void sqliteCheck() throws Exception {
        java.io.File exe = new java.io.File("/usr/bin/sqlite3");
        for (String dir : System.getenv("PATH").split(":")) if (!exe.canExecute() && new java.io.File(dir, "sqlite3").canExecute()) exe = new java.io.File(dir, "sqlite3");
        if (!exe.canExecute()) { System.out.println("SKIP sqlite3 selection check (no sqlite3)"); return; }
        for (boolean mms : new boolean[] {false, true}) {
            // rows: date unit is ms for sms, seconds for mms; ids overlap on purpose
            StringBuilder sql = new StringBuilder("create table t(_id integer, thread_id integer, date integer);");
            long unit = mms ? 1 : 1;
            long[][] rows = mms ? new long[][] {{1, 1, 100}, {2, 1, 101}, {3, 1, 101}, {4, 1, 102}, {5, 2, 101}}
                                : new long[][] {{1, 1, 100500}, {2, 1, 101000}, {3, 1, 101000}, {4, 1, 102000}, {5, 2, 101000}};
            for (long[] r : rows) sql.append("insert into t values(" + r[0] + "," + r[1] + "," + r[2] + ");");
            // cursor: the row (id 3, date 101 s / 101000 ms) -> expect older rows of thread 1 only: ids 2 and 1 (sms: 2,1)
            long before = 101000;
            String[] q = org.qindaqt.link.sms.SmsQueries.messageSelection(mms, 1, before, 3);
            String where = q[0];
            for (int i = 1; i < q.length; i++) where = where.replaceFirst("\\?", "'" + q[i] + "'"); // TEXT literal
            sql.append("select group_concat(_id) from (select _id from t where " + where + " order by date desc, _id desc);");
            Process pr = new ProcessBuilder(exe.getPath(), ":memory:", sql.toString()).redirectErrorStream(true).start();
            String out = new String(pr.getInputStream().readAllBytes()).trim();
            pr.waitFor();
            expect(out.equals("2,1"), (mms ? "mms" : "sms") + " clause with TEXT binds selects the older rows: " + out);
            // a non-whole-second cursor must still exclude rows at/after it
            if (mms) {
                q = org.qindaqt.link.sms.SmsQueries.messageSelection(true, 1, 101500, 99);
                where = q[0];
                for (int i = 1; i < q.length; i++) where = where.replaceFirst("\\?", "'" + q[i] + "'");
                pr = new ProcessBuilder(exe.getPath(), ":memory:", sql.substring(0, sql.lastIndexOf("select group_concat")) + "select group_concat(_id) from (select _id from t where " + where + " order by date desc, _id desc);").redirectErrorStream(true).start();
                out = new String(pr.getInputStream().readAllBytes()).trim();
                pr.waitFor();
                expect(out.equals("3,2,1"), "mms cursor inside a second keeps that second's row: " + out);
            }
        }
    }

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
        f.onFrame(req("sms.messages", 8, "thread", 1L, "limit", 200L, "before", lm.get("date"), "beforeKind", lm.get("kind"), "beforeId", lm.get("id")));
        expect(c.last().b.get("messages") != null && ((List<?>) c.last().b.get("messages")).size() == 60 && Boolean.FALSE.equals(c.last().b.get("more")), "paged to the end: 260 total");
        f.onFrame(req("sms.messages", 9));
        expect("bad-request".equals(c.last().b.get("code")), "thread required");

        // A zero cursor (what a D-Bus client passes for "unset") means newest, not "older than 1970".
        f.onFrame(req("sms.threads", 50, "limit", 5L, "before", 0L, "beforeId", 0L));
        expect(c.last().t.equals("sms.threads") && ((List<?>) c.last().b.get("threads")).size() == 5, "before=0 means from the newest");

        // Provider failures reach the desktop as errors, never as an empty list.
        FakeProvider broken = new FakeProvider() {
            public List<ThreadRow> threads(int l, long b, long bi) { throw new ProviderException("conversations failed: SQLiteException"); }
            public List<MessageRow> smsMessages(long th, int l, long b, long bi) { throw new IllegalStateException("x"); }
            public Map<Long, String> fingerprints() { throw new IllegalStateException("x"); }
        };
        SmsFeature fx = new SmsFeature(broken, names, sender, new EngineState());
        Ctx cx = new Ctx();
        fx.onReady(cx);
        expect(cx.last().t.equals("sms.changed"), "a failing fingerprint query does not break READY");
        fx.onFrame(req("sms.threads", 60));
        expect(cx.last().t.equals("error") && "internal".equals(cx.last().b.get("code")), "ProviderException -> error{internal}");
        fx.onFrame(req("sms.messages", 61, "thread", 1L));
        expect("internal".equals(cx.last().b.get("code")), "any RuntimeException from the provider -> error{internal}");

        // Capability probing: the first strategy throws for unsupported columns, the second works and is remembered.
        final int[] calls = {0, 0};
        org.qindaqt.link.sms.FallbackChain<String> chain = new org.qindaqt.link.sms.FallbackChain<>(Arrays.<java.util.concurrent.Callable<String>>asList(
                () -> { calls[0]++; throw new IllegalStateException("no such column: message_count"); },
                () -> { calls[1]++; return "ok"; }));
        expect("ok".equals(chain.call("t")) && "ok".equals(chain.call("t")), "fallback strategy used");
        expect(calls[0] == 1 && calls[1] == 2, "working strategy is tried first next time: " + calls[0] + "/" + calls[1]);
        try {
            new org.qindaqt.link.sms.FallbackChain<String>(Arrays.<java.util.concurrent.Callable<String>>asList(
                    () -> { throw new IllegalStateException("secret-value"); },
                    () -> { throw new UnsupportedOperationException("secret-value"); })).call("conversations");
            expect(false, "all strategies failing must throw");
        } catch (SmsProvider.ProviderException ex) {
            expect(ex.getMessage().equals("conversations failed: IllegalStateException,UnsupportedOperationException"), "reason has class names only: " + ex.getMessage());
        }

        // hydrate runs only for rows that make the page.
        final int[] hyd = {0};
        FakeProvider hp = new FakeProvider() { public MessageRow hydrate(MessageRow r) { hyd[0]++; return r; } };
        for (int i = 1; i <= 30; i++) hp.mms.add(new SmsProvider.MessageRow(i, true, "inbox", null, 100 + i, null, true, null));
        SmsFeature fh = new SmsFeature(hp, names, sender, new EngineState());
        Ctx ch2 = new Ctx();
        fh.onReady(ch2);
        fh.onFrame(req("sms.messages", 70, "thread", 1L, "limit", 5L));
        expect(hyd[0] == 5 && ((List<?>) ch2.last().b.get("messages")).size() == 5, "only the 5 kept rows were hydrated: " + hyd[0]);

        // Same-millisecond sms and mms rows with overlapping ids: paging one row at a time sees each exactly once.
        FakeProvider tie = new FakeProvider();
        for (int i = 1; i <= 3; i++) {
            tie.sms.add(new SmsProvider.MessageRow(i, false, "inbox", "+1", 9000, "s", true, null));
            tie.mms.add(new SmsProvider.MessageRow(i, true, "inbox", "+1", 9000, "m", true, null));
        }
        tie.sms.add(new SmsProvider.MessageRow(7, false, "inbox", "+1", 8000, "s", true, null));
        SmsFeature ft = new SmsFeature(tie, names, sender, new EngineState());
        Ctx ct = new Ctx();
        ft.onReady(ct);
        java.util.Set<String> seenRows = new java.util.HashSet<>();
        Object[] cur = null;
        for (int guard = 0; guard < 20; guard++) {
            Envelope q = cur == null ? req("sms.messages", 40 + guard, "thread", 1L, "limit", 1L)
                    : req("sms.messages", 40 + guard, "thread", 1L, "limit", 1L, "before", cur[0], "beforeKind", cur[1], "beforeId", cur[2]);
            ft.onFrame(q);
            List<Map<String, Object>> page = (List<Map<String, Object>>) ct.last().b.get("messages");
            if (page.isEmpty()) break;
            Map<String, Object> row = page.get(0);
            expect(seenRows.add(row.get("kind") + ":" + row.get("id")), "no row repeated across pages");
            cur = new Object[] {row.get("date"), row.get("kind"), row.get("id")};
            if (Boolean.FALSE.equals(ct.last().b.get("more"))) break;
        }
        expect(seenRows.size() == 7, "all 7 rows seen exactly once: " + seenRows.size());
        ft.onFrame(req("sms.messages", 90, "thread", 1L, "beforeKind", "xyz"));
        expect("bad-request".equals(ct.last().b.get("code")), "bad beforeKind rejected");
        f.onFrame(req("sms.messages", 91, "thread", "7"));
        expect("bad-request".equals(c.last().b.get("code")), "non-integer thread -> bad-request");
        f.onFrame(req("sms.part", 92, "part", 1.5));
        expect("bad-request".equals(c.last().b.get("code")), "non-integer part -> bad-request");
        f.onFrame(req("sms.read", 93, "thread", "7"));
        expect("bad-request".equals(c.last().b.get("code")), "non-integer read thread -> bad-request");

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
                {new ArrayList<Object>(Arrays.asList(7L)), "x"}, {new ArrayList<Object>(Arrays.asList("+15550001")), "explode"}}) {
            f.onFrame(req("sms.send", 16, "addrs", bad[0], "text", bad[1]));
            expect("bad-request".equals(c.last().b.get("code")), "invalid send rejected");
        }
        StringBuilder longText = new StringBuilder();
        for (int i = 0; i < 1601; i++) longText.append('a');
        f.onFrame(req("sms.send", 17, "addrs", new ArrayList<Object>(Arrays.asList("1")), "text", longText.toString()));
        expect("bad-request".equals(c.last().b.get("code")), "text over 1600 chars rejected");
        // Destinations: only dialable numbers; one bad recipient sends nothing.
        sender.l = null;
        for (String badAddr : new String[] {"12345", "MyBank", "+1555", "555-12", "1234567x", ""}) {
            f.onFrame(req("sms.send", 30, "addrs", new ArrayList<Object>(Arrays.asList("+15550001", badAddr)), "text", "x"));
            expect("denied".equals(c.last().b.get("code")) || "bad-request".equals(c.last().b.get("code")), "non-dialable recipient refused: " + badAddr);
            expect(sender.l == null, "nothing handed to the sender when a recipient is refused: " + badAddr);
        }
        expect("+15551234567".equals(org.qindaqt.link.sms.Dialable.normalize(" +1 (555) 123-4567 ")), "formatting stripped");
        expect(org.qindaqt.link.sms.Dialable.normalize("1234567") != null && org.qindaqt.link.sms.Dialable.normalize("123456") == null, "7 digit minimum");
        List<Object> eleven = new ArrayList<>();
        for (int i = 0; i < 11; i++) eleven.add("+1555000" + (1000 + i));
        f.onFrame(req("sms.send", 31, "addrs", eleven, "text", "x"));
        expect("bad-request".equals(c.last().b.get("code")) && sender.l == null, "more than 10 recipients rejected");
        // A platform failure before anything was sent is an error reply; after the first recipient it is an unsent event.
        FakeSender boom = new FakeSender() { public void send(long r, List<String> a2, String t2, int s2, Listener li) { throw new IllegalStateException("x"); } };
        SmsFeature fb = new SmsFeature(p, names, boom, new EngineState());
        Ctx cb = new Ctx();
        fb.onReady(cb);
        fb.onFrame(req("sms.send", 32, "addrs", new ArrayList<Object>(Arrays.asList("+15550001")), "text", "x"));
        expect("internal".equals(cb.last().b.get("code")), "RuntimeException from the sender becomes error{internal}");
        FakeSender partial = new FakeSender() { public void send(long r, List<String> a2, String t2, int s2, Listener li) { li.onUnsent(r, "unknown", Arrays.asList(1)); } };
        SmsFeature fp = new SmsFeature(p, names, partial, new EngineState());
        Ctx cp = new Ctx();
        fp.onReady(cp);
        fp.onFrame(req("sms.send", 33, "addrs", new ArrayList<Object>(Arrays.asList("+15550001", "+15550002")), "text", "x"));
        fp.onTick(System.currentTimeMillis());
        expect(cp.last().t.equals("sms.sent") && "failed".equals(cp.last().b.get("state")) && Arrays.asList(1).equals(cp.last().b.get("unsent")), "partial send reports the unsent recipients");

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
        sqliteCheck();
        System.out.println("SmsTest OK");
    }
}
