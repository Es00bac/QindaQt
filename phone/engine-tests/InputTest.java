import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.input.InputFeature;
import org.qindaqt.link.input.InputFrames;
import org.qindaqt.link.input.KeyChord;
import org.qindaqt.link.input.TouchGestures;
import org.qindaqt.link.macro.MacroFeature;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.Json;

/** Pure-Java tests for gesture-to-frame logic, frame bounds, chords and the input/macro handlers. */
public final class InputTest {
    static int checks;

    static void expect(boolean ok, String what) {
        checks++;
        if (!ok) { System.err.println("FAIL: " + what); System.exit(1); }
    }

    /** Records what the gesture engine produced. */
    static final class Rec implements TouchGestures.Sink {
        final List<String> out = new ArrayList<>();
        int haptics;
        public void pointer(double dx, double dy) { out.add(String.format("move %.1f %.1f", dx, dy)); }
        public void button(String b, String s) { out.add(b + " " + s); }
        public void scroll(double dx, double dy, boolean end) { out.add(String.format("scroll %.1f %.1f %s", dx, dy, end)); }
        public void haptic() { haptics++; }
    }

    static final class Ctx implements FeatureContext {
        final List<Envelope> sent = new ArrayList<>();
        long id;
        boolean peerInput = true;
        public boolean send(Envelope e) { sent.add(e); return true; }
        public long nextId() { return ++id; }
        public boolean peerHas(String c) { return peerInput; }
        public void replyOk(long i) { }
        public void replyError(long i, String c, String m) { }
        public void wake() { }
        List<String> types() { List<String> t = new ArrayList<>(); for (Envelope e : sent) t.add(e.t); return t; }
    }

    static Map<String, Object> m(Object... kv) {
        Map<String, Object> r = new LinkedHashMap<>();
        for (int i = 0; i < kv.length; i += 2) r.put((String) kv[i], kv[i + 1]);
        return r;
    }

    static void gestures() {
        Rec r = new Rec();
        TouchGestures g = new TouchGestures(r);
        g.setSensitivity(2.0);
        // Tap: down and up quickly, no movement -> left click.
        g.update(0, 1, 100, 100, 0, 0);
        g.update(100, 0, 0, 0, 0, 0);
        expect(r.out.equals(Arrays.asList("left click")), "tap is a left click: " + r.out);
        // Move: beyond slop, scaled by sensitivity, no click on release.
        r.out.clear();
        g.update(1000, 1, 100, 100, 0, 0);
        g.update(1100, 1, 120, 100, 0, 0);
        g.update(1200, 0, 0, 0, 0, 0);
        expect(r.out.equals(Arrays.asList("move 40.0 0.0")), "move scaled and no click: " + r.out);
        // Hold then drag: left down + haptic, movement, then up.
        r.out.clear();
        g.update(2000, 1, 50, 50, 0, 0);
        g.update(2400, 1, 50, 50, 0, 0); // still: HOLD_MS passed
        expect(r.out.equals(Arrays.asList("left down")) && r.haptics == 1 && g.dragging(), "hold starts a drag: " + r.out);
        g.update(2500, 1, 60, 50, 0, 0);
        g.update(2600, 0, 0, 0, 0, 0);
        expect(r.out.equals(Arrays.asList("left down", "move 20.0 0.0", "left up")), "drag moves and releases: " + r.out);
        // Two-finger tap: right click.
        r.out.clear();
        g.update(3000, 2, 100, 100, 200, 100);
        g.update(3100, 0, 0, 0, 0, 0);
        expect(r.out.equals(Arrays.asList("right click")), "two-finger tap is a right click: " + r.out);
        // Two-finger scroll: natural scrolling, fingers up -> positive dy (toward the end).
        r.out.clear();
        g.update(4000, 2, 100, 300, 200, 300);
        g.update(4100, 2, 100, 260, 200, 260);
        g.update(4200, 0, 0, 0, 0, 0);
        expect(r.out.size() == 2 && r.out.get(0).equals("scroll 0.0 20.0 false") && r.out.get(1).equals("scroll 0.0 0.0 true"),
                "scroll natural direction and end marker: " + r.out);
        g.setNaturalScroll(false);
        r.out.clear();
        g.update(5000, 2, 100, 300, 200, 300);
        g.update(5100, 2, 100, 260, 200, 260);
        g.update(5200, 0, 0, 0, 0, 0);
        expect(r.out.get(0).equals("scroll 0.0 -20.0 false"), "scroll non-natural: " + r.out);
        // reset() releases a held drag (no stuck button when the screen is left).
        r.out.clear();
        g.update(6000, 1, 10, 10, 0, 0);
        g.update(6400, 1, 10, 10, 0, 0);
        g.reset();
        expect(r.out.equals(Arrays.asList("left down", "left up")) && !g.dragging(), "reset releases drag: " + r.out);
        // Rate: 1 ms touch samples produce at most one move per 8 ms (120/s).
        r.out.clear();
        g.update(7000, 1, 0, 0, 0, 0);
        for (int i = 1; i <= 1000; i++) g.update(7000 + i, 1, i * 2, 0, 0, 0);
        int moves = 0;
        double total = 0;
        for (String s : r.out) if (s.startsWith("move")) { moves++; total += Double.parseDouble(s.split(" ")[1]); }
        expect(moves <= 125 && moves >= 100, "movement coalesced to ~120/s: " + moves);
        expect(Math.abs(total - 2000 * 1.0 * 2.0) < 40, "coalescing keeps the distance: " + total);
        // A third finger is ignored (treated as two).
        g.update(8500, 0, 0, 0, 0, 0);
        r.out.clear();
        g.update(9000, 3, 1, 1, 2, 2);
        g.update(9050, 0, 0, 0, 0, 0);
        expect(r.out.equals(Arrays.asList("right click")), "three fingers behave as two: " + r.out);
        // Sensitivity is clamped.
        g.setSensitivity(100);
        expect(g.sensitivity() == 4.0, "sensitivity upper clamp");
        g.setSensitivity(Double.NaN);
        expect(g.sensitivity() == 1.6, "sensitivity NaN falls back");
    }

    static void frames() {
        Envelope p = InputFrames.pointer(5000, Double.NaN);
        expect(((Double) p.b.get("dx")) == 1000.0 && ((Double) p.b.get("dy")) == 0.0, "pointer clamped, NaN -> 0");
        expect(InputFrames.button("back", "down") == null && InputFrames.button("left", "tap") == null, "unknown button/state refused");
        expect(InputFrames.button("left", "click") != null, "left click ok");
        Envelope s = InputFrames.scroll(0, 9999, true);
        expect(((Double) s.b.get("dy")) == 500.0 && Boolean.TRUE.equals(s.b.get("end")), "smooth scroll clamped with end");
        expect(((Long) InputFrames.scrollDiscrete(0, 99).b.get("dy")) == 20L, "discrete clamped to 20");
        expect(InputFrames.key("Bogus", "tap") == null && InputFrames.key("Escape", "tap") != null, "closed key-name set");
        expect(InputFrames.keysym(0x1008ff2aL, "tap") == null && InputFrames.keysym(0x1008ff2fL, "tap") == null
                && InputFrames.keysym(0x1008ffa7L, "tap") == null && InputFrames.keysym(0x1008ff61L, "tap") == null,
                "session-control XF86 keysyms refused");
        expect(InputFrames.keysym(0x1008ff13L, "tap") != null, "volume keysym allowed");
        expect(InputFrames.keysym(7, "down") == null && InputFrames.keysym(0x01000000L | 0x20ac, "tap") != null, "keysym ranges");
        expect(InputFrames.text("") == null && InputFrames.text("hi") != null, "text non-empty");
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 4097; i++) big.append('x');
        expect(InputFrames.text(big.toString()) == null, "text over 4096 bytes refused");
        List<String> parts = InputFrames.splitText("a€😀bcdefgh", 8);
        StringBuilder joined = new StringBuilder();
        for (String part : parts) {
            expect(part.getBytes(java.nio.charset.StandardCharsets.UTF_8).length <= 8, "chunk within byte limit: " + part);
            joined.append(part);
        }
        expect(joined.toString().equals("a€😀bcdefgh"), "chunks rejoin without cutting a code point");
        // Every frame the builders produce survives the protocol envelope check (type pattern, size).
        for (Envelope e : Arrays.asList(p, s, InputFrames.text("x"), InputFrames.key("Tab", "down")))
            expect(Json.write(e.toObject()).length() < 256, "frames stay small");
    }

    static void chords() {
        List<Envelope> f = KeyChord.frames("Tab", KeyChord.ALT | KeyChord.CTRL);
        List<String> seq = new ArrayList<>();
        for (Envelope e : f) seq.add(e.b.get("name") + ":" + e.b.get("state"));
        expect(seq.equals(Arrays.asList("Ctrl:down", "Alt:down", "Tab:tap", "Alt:up", "Ctrl:up")), "chord order: " + seq);
        expect(KeyChord.frames("Nope", 0).isEmpty(), "unknown key gives no frames");
        List<Envelope> c = KeyChord.frames(0x20ac, KeyChord.CTRL);
        expect(c.size() == 3 && ((Long) c.get(1).b.get("keysym")) == (0x01000000L | 0x20ac), "unicode keysym");
        List<Envelope> a = KeyChord.frames('a', 0);
        expect(a.size() == 1 && ((Long) a.get(0).b.get("keysym")) == 97L, "plain char tap");
    }

    static void inputFeature() throws IOException {
        final long[] now = {0};
        InputFeature f = new InputFeature(() -> now[0]);
        Ctx c = new Ctx();
        f.begin();
        expect(f.state() == InputFeature.State.OFFLINE, "begin while disconnected stays offline");
        f.onReady(c);
        // Nothing but begin is sent before the desktop says active.
        f.begin();
        f.click("left");
        f.sendKey("Escape", 0);
        f.onTick(0);
        expect(c.types().equals(Arrays.asList("input.begin")) && c.sent.get(0).id == 1, "only input.begin before approval: " + c.types());
        expect(f.state() == InputFeature.State.PENDING, "pending after begin");
        // Frames arriving while pending are not queued.
        f.onFrame(Envelope.of("input.state", m("state", "active")));
        expect(f.state() == InputFeature.State.ACTIVE, "active from input.state");
        c.sent.clear();
        f.click("right");
        f.sendKey("Tab", KeyChord.SHIFT);
        f.sendText("hello");
        f.onTick(0);
        expect(c.types().equals(Arrays.asList("input.button", "input.key", "input.key", "input.key", "input.text")),
                "queued frames then one text chunk: " + c.types());
        // Touch: a tap through the real engine produces a click frame.
        c.sent.clear();
        now[0] = 1000;
        f.touch(1, 10, 10, 0, 0);
        now[0] = 1050;
        f.touch(0, 0, 0, 0, 0);
        f.onTick(0);
        expect(c.types().equals(Arrays.asList("input.button")) && "click".equals(c.sent.get(0).b.get("state")), "tap -> click frame");
        // Long text is chunked and paced one chunk per tick.
        c.sent.clear();
        StringBuilder t = new StringBuilder();
        for (int i = 0; i < 2500; i++) t.append('z');
        f.sendText(t.toString());
        f.onTick(0);
        expect(c.sent.size() == 1, "first chunk on first tick");
        f.onTick(0);
        f.onTick(0);
        expect(c.sent.size() == 3, "one chunk per tick, 2500 bytes -> 3 chunks of at most 1024");
        // Lock: queued frames are dropped while not active, drag released by end().
        f.onFrame(Envelope.of("input.state", m("state", "locked")));
        expect(f.state() == InputFeature.State.LOCKED, "locked state");
        c.sent.clear();
        f.click("left");
        f.onTick(0);
        expect(c.sent.isEmpty(), "nothing sent while locked");
        f.onFrame(Envelope.of("input.state", m("state", "active")));
        // end(): a held drag is released first, then input.end.
        now[0] = 3000;
        f.touch(1, 10, 10, 0, 0);
        now[0] = 3400;
        f.touch(1, 10, 10, 0, 0);
        f.end();
        f.onTick(0);
        List<String> ty = c.types();
        expect(ty.size() == 3 && ty.get(0).equals("input.button") && "down".equals(c.sent.get(0).b.get("state"))
                && "up".equals(c.sent.get(1).b.get("state")) && ty.get(2).equals("input.end"),
                "drag released before input.end: " + ty);
        // Queue pressure never evicts an up/click: 700 pointer frames on each side of a key-up and a click.
        f.begin();
        f.onFrame(Envelope.of("input.state", m("state", "active")));
        f.onTick(0);
        c.sent.clear();
        for (int i = 0; i < 700; i++) f.pointerForTest(1, 0);
        f.click("left");
        for (int i = 0; i < 700; i++) f.pointerForTest(1, 0);
        f.sendKeyFrameForTest("Shift", "up");
        for (int i = 0; i < 700; i++) f.pointerForTest(1, 0);
        f.onTick(0);
        boolean sawClick = false, sawUp = false;
        for (Envelope e : c.sent) {
            if (e.t.equals("input.button")) sawClick = true;
            if (e.t.equals("input.key") && "up".equals(e.b.get("state"))) sawUp = true;
        }
        expect(sawClick && sawUp, "click and key-up survive a full queue");
        expect(c.sent.size() <= 512, "queue bounded: " + c.sent.size());
        // Revocation from the desktop.
        f.begin();
        f.onTick(0);
        f.onFrame(Envelope.of("input.state", m("state", "active")));
        f.onFrame(Envelope.of("input.end", m("reason", "revoked")));
        expect(f.state() == InputFeature.State.REVOKED, "revoked state");
        f.begin();
        f.onTick(0);
        expect(c.types().get(c.types().size() - 1).equals("input.begin"), "begin again after revoke asks again");
        // A desktop without the capability gets nothing.
        c.sent.clear();
        c.peerInput = false;
        f.begin();
        f.onTick(0);
        expect(c.sent.isEmpty(), "no frames to a desktop without the input capability");
        f.onClosed();
        expect(f.state() == InputFeature.State.OFFLINE, "closed -> offline");
        expect(f.json().contains("\"offline\""), "json state");
    }

    static void macros() throws IOException {
        MacroFeature mf = new MacroFeature();
        Ctx c = new Ctx();
        mf.onReady(c);
        List<Object> rows = new ArrayList<>();
        rows.add(m("id", "copy", "name", "Copy"));
        rows.add(m("id", "bad id", "name", "Spaces"));
        rows.add(m("id", "long", "name", "x".repeat(81)));
        rows.add("junk");
        rows.add(m("id", "paste", "name", "Paste", "steps", "never shown"));
        mf.onFrame(Envelope.of("macro.list", m("rev", "3", "macros", rows)));
        expect(mf.macros().size() == 2 && mf.macros().get(1).id.equals("paste"), "invalid rows dropped");
        List<Object> many = new ArrayList<>();
        for (int i = 0; i < 100; i++) many.add(m("id", "m" + i, "name", "M" + i));
        mf.onFrame(Envelope.of("macro.list", m("macros", many)));
        expect(mf.macros().size() == 64, "at most 64 macros");
        mf.onFrame(Envelope.of("macro.list", m("macros", rows)));
        expect(!mf.json().contains("steps") && !mf.json().contains("never shown"), "only id and name reach the UI");
        mf.run("bad id");
        mf.onTick(0);
        expect(c.sent.isEmpty(), "invalid id never sent");
        mf.run("copy");
        mf.run("paste"); // ignored while one is running
        mf.onTick(0);
        expect(c.types().equals(Arrays.asList("macro.run")) && "copy".equals(c.sent.get(0).b.get("id")), "run sent once: " + c.types());
        expect(mf.json().contains("\"running\":\"copy\""), "running shown");
        mf.onFrame(Envelope.of("macro.state", m("id", "copy", "state", "done")));
        expect(mf.json().contains("\"running\":\"\"") && mf.json().contains("\"last\":\"copy\""), "done clears running");
        mf.run("paste");
        mf.cancel();
        mf.onTick(0);
        expect(c.types().equals(Arrays.asList("macro.run", "macro.run", "macro.cancel")), "run then cancel: " + c.types());
        mf.onFrame(Envelope.of("macro.state", m("id", "paste", "state", "failed", "reason", "input-unavailable")));
        expect(mf.json().contains("\"runState\":\"failed\"") && mf.json().contains("input-unavailable"), "failure reason shown");
        mf.onFrame(Envelope.of("macro.state", m("id", "paste", "state", "exploded")));
        expect(mf.json().contains("\"runState\":\"failed\""), "unknown state ignored");
        mf.onClosed();
        mf.run("copy");
        mf.onTick(0);
        expect(c.sent.size() == 3, "nothing sent when disconnected");
    }

    public static void main(String[] a) throws Exception {
        gestures();
        frames();
        chords();
        inputFeature();
        macros();
        System.out.println("input tests OK (" + checks + " checks)");
    }
}
