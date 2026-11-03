import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.nio.file.Files;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicReference;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.FrameCodec;
import org.qindaqt.link.vault.*;

/** Vault part 2 on the phone: wire steps, the confirmation flow and the request/reply path. Throwaway strings only. */
public final class VaultTypeTests {
    static int checks;

    static void check(boolean ok, String what) {
        checks++;
        if (!ok) { System.err.println("FAIL: " + what); System.exit(1); }
    }

    static List<TypingStep> steps(VaultSession s, String id, VaultTypingSource.Field f, VaultTypingSource[] keep) throws Exception {
        keep[0] = new VaultTypingSource(s, id, f);
        return keep[0].steps();
    }

    static void wireSteps() throws Exception {
        VaultSession s = VaultTests.fresh();
        VaultRecord r = VaultTests.rec("w", "user€", "pw-1");
        r.autotype = "{USERNAME}{TAB}{DELAY 100}{PASSWORD}{KEY ESC}{KEY DOWN}{ENTER}";
        String id = s.put(r);
        VaultTypingSource[] k = new VaultTypingSource[1];
        List<TypingStep> st = steps(s, id, VaultTypingSource.Field.AUTOTYPE, k);
        VaultWireSteps.validate(st);
        List<Map<String, Object>> w = VaultWireSteps.encode(st);
        check(w.size() == 7, "seven wire steps");
        check("user€".equals(w.get(0).get("text")) && "Tab".equals(w.get(1).get("key")) && Long.valueOf(100).equals(w.get(2).get("delayMs"))
            && "pw-1".equals(w.get(3).get("text")) && "Escape".equals(w.get(4).get("key")) && "Down".equals(w.get(5).get("key"))
            && "Enter".equals(w.get(6).get("key")), "wire vocabulary");
        k[0].close();

        // Bounds: text over 1024 bytes splits on character boundaries; delays over 2000 split; > 16 steps refused.
        VaultRecord big = VaultTests.rec("big", "x", "y");
        big.setPassword("€".repeat(500));   // 1500 bytes
        big.autotype = "{PASSWORD}{DELAY 5000}";
        id = s.put(big);
        st = steps(s, id, VaultTypingSource.Field.AUTOTYPE, k);
        VaultWireSteps.validate(st);
        w = VaultWireSteps.encode(st);
        check(w.size() == 2 + 3, "1500 bytes -> 2 text chunks, 5000 ms -> 3 delays: " + w.size());
        String a = (String) w.get(0).get("text"), b = (String) w.get(1).get("text");
        check(a.getBytes("UTF-8").length <= 1024 && b.getBytes("UTF-8").length <= 1024 && (a + b).equals("€".repeat(500)), "split keeps characters whole");
        for (int i = 2; i < 5; i++) check(((Long) w.get(i).get("delayMs")) <= 2000, "delay bound");
        k[0].close();

        VaultRecord many = VaultTests.rec("many", "x", "y");
        many.autotype = "{TAB}".repeat(17);
        id = s.put(many);
        st = steps(s, id, VaultTypingSource.Field.AUTOTYPE, k);
        try { VaultWireSteps.validate(st); check(false, "17 steps"); } catch (VaultWireSteps.Unsupported e) { check(e.getMessage().contains("too long"), "too long refused"); }
        k[0].close();

        VaultRecord home = VaultTests.rec("home", "x", "y");
        home.autotype = "{USERNAME}{KEY HOME}";
        id = s.put(home);
        st = steps(s, id, VaultTypingSource.Field.AUTOTYPE, k);
        try { VaultWireSteps.validate(st); check(false, "HOME"); } catch (VaultWireSteps.Unsupported e) { check(e.getMessage().contains("HOME") && !e.getMessage().contains("x"), "unsupported key named"); }
        k[0].close();

        // Control characters in literal text stay text (the desktop skips them); they are never turned into keys.
        VaultRecord ctl = VaultTests.rec("ctl", "x", "y");
        ctl.autotype = "a\tb";
        id = s.put(ctl);
        st = steps(s, id, VaultTypingSource.Field.AUTOTYPE, k);
        w = VaultWireSteps.encode(st);
        check(w.size() == 1 && "a\tb".equals(w.get(0).get("text")) && !w.get(0).containsKey("key"), "tab inside text stays text");
        k[0].close();

        VaultRecord empty = VaultTests.rec("empty", "", "y");
        empty.autotype = "{USERNAME}";
        id = s.put(empty);
        st = steps(s, id, VaultTypingSource.Field.AUTOTYPE, k);
        try { VaultWireSteps.validate(st); check(false, "nothing"); } catch (VaultWireSteps.Unsupported e) { check(e.getMessage().contains("nothing"), "nothing to type refused"); }
        k[0].close();
    }

    /** A link that records and lets the test answer by hand. */
    static final class FakeLink implements VaultTypeFlow.DesktopLink {
        VaultTypeFlow.FocusCallback focus;
        VaultTypeFlow.ResultCallback result;
        VaultTypingSource sent;
        String sentToken;
        int focusCalls, sendCalls;
        public void requestFocus(VaultTypeFlow.FocusCallback cb) { focusCalls++; focus = cb; }
        public void sendType(String token, VaultTypingSource src, VaultTypeFlow.ResultCallback cb) { sendCalls++; sentToken = token; sent = src; result = cb; }
    }

    static void flow() throws Exception {
        VaultSession s = VaultTests.fresh();
        VaultRecord r = VaultTests.rec("f", "user", "pw-F");
        String id = s.put(r);
        final long[] now = {1000};
        FakeLink link = new FakeLink();
        VaultTypeFlow f = new VaultTypeFlow(link, () -> s, () -> now[0]);

        // Happy path: focus -> confirm -> send -> ok.
        f.begin(id, VaultTypingSource.Field.PASSWORD);
        check(f.phase() == VaultTypeFlow.Phase.REQUESTING && link.focusCalls == 1, "asks the desktop first");
        check(link.sendCalls == 0, "nothing sent before the user confirmed");
        link.focus.onFocus("tok1", "org.example.term", "Terminal");
        check(f.phase() == VaultTypeFlow.Phase.CONFIRM && f.app().equals("org.example.term") && f.title().equals("Terminal"), "target shown");
        f.confirm();
        check(f.phase() == VaultTypeFlow.Phase.SENDING && link.sendCalls == 1 && "tok1".equals(link.sentToken), "sent with the confirmed token");
        check(link.sent.steps().size() == 1 && "pw-F".equals(link.sent.steps().get(0).textString()), "password step built after confirm");
        link.sent.close();
        link.result.onDone();
        check(f.phase() == VaultTypeFlow.Phase.DONE, "done");
        f.confirm();
        check(link.sendCalls == 1, "a finished flow cannot send again");

        // The confirmation lapses after 30 s: nothing is sent.
        f.begin(id, VaultTypingSource.Field.USERNAME);
        link.focus.onFocus("tok2", "a", "t");
        now[0] += VaultTypeFlow.CONFIRM_LAPSE_MS + 1;
        f.confirm();
        check(f.phase() == VaultTypeFlow.Phase.FAILED && link.sendCalls == 1 && f.message().contains("lapsed"), "lapsed confirmation sends nothing");
        f.begin(id, VaultTypingSource.Field.USERNAME);
        link.focus.onFocus("tok3", "a", "t");
        now[0] += VaultTypeFlow.CONFIRM_LAPSE_MS + 1;
        f.poll();
        check(f.phase() == VaultTypeFlow.Phase.FAILED, "poll lapses the sheet");

        // Locked: steps are built only while unlocked; the UI unlocks and confirms again within the window.
        VaultSession[] holder = {s};
        VaultTypeFlow g = new VaultTypeFlow(link, () -> holder[0], () -> now[0]);
        g.begin(id, VaultTypingSource.Field.PASSWORD);
        link.focus.onFocus("tok4", "a", "t");
        s.lock();
        g.confirm();
        check(g.phase() == VaultTypeFlow.Phase.NEED_UNLOCK && link.sendCalls == 1, "locked vault: unlock first, nothing built or sent");
        VaultSession s2 = VaultTests.fresh();
        String id2 = s2.put(VaultTests.rec("g", "user2", "pw-G"));
        VaultTypeFlow h = new VaultTypeFlow(link, () -> s2, () -> now[0]);
        h.begin(id2, VaultTypingSource.Field.PASSWORD);
        link.focus.onFocus("tok5", "a", "t");
        VaultSession locked = VaultTests.fresh();
        locked.lock();
        VaultSession[] cur = {locked};
        VaultTypeFlow u = new VaultTypeFlow(link, () -> cur[0], () -> now[0]);
        u.begin(id2, VaultTypingSource.Field.PASSWORD);
        link.focus.onFocus("tok6", "a", "t");
        u.confirm();
        check(u.phase() == VaultTypeFlow.Phase.NEED_UNLOCK, "locked session needs unlock");
        cur[0] = s2;
        u.confirm();
        check(u.phase() == VaultTypeFlow.Phase.SENDING && link.sendCalls == 2 && "tok6".equals(link.sentToken), "confirm after unlock sends");
        link.sent.close();

        // Refusals from the desktop become plain messages and never echo the desktop's text.
        for (String code : new String[] {"target-changed", "locked", "unverified", "busy", "denied", "internal", "disconnected"}) {
            FakeLink l2 = new FakeLink();
            VaultTypeFlow x = new VaultTypeFlow(l2, () -> s2, () -> now[0]);
            x.begin(id2, VaultTypingSource.Field.PASSWORD);
            l2.focus.onFocus("t", "a", "t");
            x.confirm();
            l2.sent.close();
            l2.result.onError(code);
            check(x.phase() == VaultTypeFlow.Phase.FAILED && x.message().equals(VaultTypeFlow.messageFor(code)) && !x.message().isEmpty(), "error " + code);
        }
        check(VaultTypeFlow.messageFor("target-changed").contains("nothing more was typed"), "target-changed explained");
        FakeLink l3 = new FakeLink();
        VaultTypeFlow y = new VaultTypeFlow(l3, () -> s2, () -> now[0]);
        y.begin(id2, VaultTypingSource.Field.PASSWORD);
        l3.focus.onError("unverified");
        check(y.phase() == VaultTypeFlow.Phase.FAILED && l3.sendCalls == 0, "focus refusal: nothing to confirm");

        // Cancel: late callbacks are ignored and nothing is sent.
        y.begin(id2, VaultTypingSource.Field.PASSWORD);
        VaultTypeFlow.FocusCallback late = l3.focus;
        y.cancel();
        late.onFocus("late", "a", "t");
        check(y.phase() == VaultTypeFlow.Phase.IDLE, "cancel ignores a late focus reply");
        y.confirm();
        check(l3.sendCalls == 0, "nothing sent after cancel");

        // A malformed autotype closes its source and fails.
        VaultRecord bad = VaultTests.rec("bad", "u", "p");
        bad.autotype = "{USERNAME}{KEY HOME}";
        String bid = s2.put(bad);
        FakeLink l4 = new FakeLink();
        VaultTypeFlow z = new VaultTypeFlow(l4, () -> s2, () -> now[0]);
        z.begin(bid, VaultTypingSource.Field.AUTOTYPE);
        l4.focus.onFocus("t", "a", "t");
        z.confirm();
        check(z.phase() == VaultTypeFlow.Phase.FAILED && l4.sendCalls == 0 && z.message().contains("HOME"), "unsupported key refused before sending");
    }

    /** Like FeatureTest.start, for any handler: a pinned session that is READY once the desktop hello was sent. */
    static Object[] start(org.qindaqt.link.engine.FeatureHandler f, List<String> peerCaps, File dir, String pinName) throws Exception {
        org.qindaqt.link.engine.PinStore pins = new org.qindaqt.link.engine.PinStore(new File(dir, pinName));
        pins.set(org.qindaqt.link.proto.Pairing.hex(FeatureTest.D), "d");
        java.net.ServerSocket ss = new java.net.ServerSocket(0, 1, java.net.InetAddress.getLoopbackAddress());
        final Socket ps = new Socket(java.net.InetAddress.getLoopbackAddress(), ss.getLocalPort());
        Socket ds = ss.accept();
        ss.close();
        org.qindaqt.link.engine.Link link = new org.qindaqt.link.engine.Link() {
            public InputStream in() throws java.io.IOException { return ps.getInputStream(); }
            public OutputStream out() throws java.io.IOException { return ps.getOutputStream(); }
            public byte[] peerSpki() { return FeatureTest.D; }
            public byte[] ownSpki() { return FeatureTest.P; }
            public void close() { try { ps.close(); } catch (java.io.IOException e) { /* ignore */ } }
        };
        org.qindaqt.link.engine.EngineState st = new org.qindaqt.link.engine.EngineState();
        org.qindaqt.link.engine.Session sess = new org.qindaqt.link.engine.Session(link, pins,
            new org.qindaqt.link.engine.FeatureRegistry().add(f), new org.qindaqt.link.engine.DeviceInfo("t", "t", "a", "0"), st);
        Thread t = new Thread(sess::run);
        t.start();
        FrameCodec dec = new FrameCodec();
        Envelope hello = SessionTest.read(ds.getInputStream(), dec);
        SessionTest.write(ds.getOutputStream(), Envelope.of("hello", SessionTest.m("proto", 1L, "role", "desktop", "name", "x",
            "model", "m", "os", "o", "app", "0", "caps", new ArrayList<Object>(peerCaps), "trusted", true)));
        // A ping answered means the hello was processed and the handlers are READY.
        SessionTest.write(ds.getOutputStream(), new Envelope("ping", 1, 0, null));
        check(SessionTest.read(ds.getInputStream(), dec).t.equals("pong"), "session READY");
        return new Object[] {sess, t, ds, dec, hello};
    }

    static boolean until(java.util.function.BooleanSupplier c) throws Exception {
        for (long end = System.currentTimeMillis() + 5000; System.currentTimeMillis() < end; Thread.sleep(10)) if (c.getAsBoolean()) return true;
        return false;
    }

    @SuppressWarnings("unchecked")
    static void featureOverSession() throws Exception {
        new SecureRandom().nextBytes(FeatureTest.D);
        new SecureRandom().nextBytes(FeatureTest.P);
        File dir = Files.createTempDirectory("qlink-v").toFile();
        VaultTypeFeature feature = new VaultTypeFeature();
        Object[] r = start(feature, Arrays.asList("vault", "status"), dir, "pinV");
        Socket ds = (Socket) r[2];
        InputStream in = ds.getInputStream();
        OutputStream out = ds.getOutputStream();
        FrameCodec dec = (FrameCodec) r[3];
        check(((Envelope) r[4]).b.get("caps").equals(Arrays.asList("vault")), "hello advertises vault");

        VaultSession s = VaultTests.fresh();
        String id = s.put(VaultTests.rec("e", "user-E", "pw-E"));
        VaultTypeFlow flow = new VaultTypeFlow(feature, () -> s, System::currentTimeMillis);
        flow.begin(id, VaultTypingSource.Field.AUTOTYPE);
        Envelope req = SessionTest.read(in, dec);
        check(req.t.equals("desktop.focus") && req.id != 0, "desktop.focus request sent");
        SessionTest.write(out, new Envelope("desktop.focus", 0, req.id, SessionTest.m("token", "tk-1", "app", "org.example.term", "title", "Terminal")));
        check(until(() -> flow.phase() == VaultTypeFlow.Phase.CONFIRM) && flow.title().equals("Terminal"), "typed reply reaches the flow");
        flow.confirm();
        Envelope type = SessionTest.read(in, dec);
        check(type.t.equals("vault.type") && "tk-1".equals(type.b.get("token")), "vault.type carries the confirmed token");
        List<Object> steps = (List<Object>) type.b.get("steps");
        check(steps.size() == 4 && "user-E".equals(((Map<String, Object>) steps.get(0)).get("text")) && "Tab".equals(((Map<String, Object>) steps.get(1)).get("key"))
            && "pw-E".equals(((Map<String, Object>) steps.get(2)).get("text")) && "Enter".equals(((Map<String, Object>) steps.get(3)).get("key")), "default autotype on the wire");
        SessionTest.write(out, new Envelope("ok", 0, type.id, null));
        check(until(() -> flow.phase() == VaultTypeFlow.Phase.DONE), "ok reaches the flow");

        // An error reply with re is routed to the requester, not swallowed by the core.
        flow.begin(id, VaultTypingSource.Field.PASSWORD);
        req = SessionTest.read(in, dec);
        SessionTest.write(out, new Envelope("error", 0, req.id, SessionTest.m("code", "unverified", "message", "x")));
        check(until(() -> flow.phase() == VaultTypeFlow.Phase.FAILED) && flow.message().equals(VaultTypeFlow.messageFor("unverified")), "error reply routed");

        // target-changed after a confirmed send.
        flow.begin(id, VaultTypingSource.Field.PASSWORD);
        req = SessionTest.read(in, dec);
        SessionTest.write(out, new Envelope("desktop.focus", 0, req.id, SessionTest.m("token", "tk-2", "app", "a", "title", "t")));
        check(until(() -> flow.phase() == VaultTypeFlow.Phase.CONFIRM), "second focus");
        flow.confirm();
        type = SessionTest.read(in, dec);
        SessionTest.write(out, new Envelope("error", 0, type.id, SessionTest.m("code", "target-changed", "message", "x")));
        check(until(() -> flow.phase() == VaultTypeFlow.Phase.FAILED) && flow.message().contains("nothing more was typed"), "target-changed shown");

        // A disconnect while a send is outstanding fails the flow.
        flow.begin(id, VaultTypingSource.Field.PASSWORD);
        req = SessionTest.read(in, dec);
        SessionTest.write(out, new Envelope("desktop.focus", 0, req.id, SessionTest.m("token", "tk-3", "app", "a", "title", "t")));
        check(until(() -> flow.phase() == VaultTypeFlow.Phase.CONFIRM), "third focus");
        flow.confirm();
        SessionTest.read(in, dec);
        ds.close();
        check(until(() -> flow.phase() == VaultTypeFlow.Phase.FAILED) && flow.message().equals(VaultTypeFlow.messageFor("disconnected")), "disconnect fails the flow");
        ((Thread) r[1]).join(5000);

        // Not connected: asking fails at once and sends nothing.
        VaultTypeFlow idle = new VaultTypeFlow(feature, () -> s, System::currentTimeMillis);
        idle.begin(id, VaultTypingSource.Field.PASSWORD);
        check(idle.phase() == VaultTypeFlow.Phase.FAILED && idle.message().equals(VaultTypeFlow.messageFor("not-connected")), "no session: not connected");

        // A desktop that lacks the vault capability: the engine refuses the frame.
        VaultTypeFeature f2 = new VaultTypeFeature();
        Object[] r2 = start(f2, Arrays.asList("status"), dir, "pinV2");
        VaultTypeFlow nocap = new VaultTypeFlow(f2, () -> s, System::currentTimeMillis);
        nocap.begin(id, VaultTypingSource.Field.PASSWORD);
        check(until(() -> nocap.phase() == VaultTypeFlow.Phase.FAILED) && nocap.message().equals(VaultTypeFlow.messageFor("not-connected")), "peer without vault: refused");
        ((Socket) r2[2]).close();
        ((Thread) r2[1]).join(5000);
    }

    public static void main(String[] a) throws Exception {
        wireSteps();
        flow();
        featureOverSession();
        System.out.println("VaultTypeTests ok: " + checks + " checks");
    }
}
