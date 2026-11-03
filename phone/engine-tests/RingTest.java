import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.ring.RingFeature;
import org.qindaqt.link.ring.RingSink;

/** Ring feature with a fake sink: nothing here can ring a phone. */
public final class RingTest {
    static int checks;

    static void expect(boolean ok, String what) {
        checks++;
        if (!ok) { System.err.println("FAIL: " + what); System.exit(1); }
    }

    static final class Sink implements RingSink {
        int starts, stops;
        boolean fail;
        Runnable userStop;
        public String start(Runnable r) {
            if (fail) throw new IllegalStateException("no audio");
            starts++;
            userStop = r;
            return "alarm,no-dnd-access";
        }
        public void stop() { stops++; }
    }

    static final class Ctx implements FeatureContext {
        final List<String> replies = new ArrayList<>();
        int wakes;
        public boolean send(Envelope e) { return true; }
        public long nextId() { return 1; }
        public boolean peerHas(String c) { return true; }
        public void replyOk(long id) { replies.add("ok" + id); }
        public void replyError(long id, String c, String m) { replies.add(c + id); }
        public void wake() { wakes++; }
    }

    static Envelope ring(long id, boolean stop) {
        java.util.Map<String, Object> b = new java.util.LinkedHashMap<>();
        if (stop) b.put("stop", Boolean.TRUE);
        return new Envelope("ring", id, 0, b);
    }

    public static void main(String[] a) throws IOException {
        Sink s = new Sink();
        EngineState st = new EngineState();
        Ctx c = new Ctx();
        RingFeature f = new RingFeature(s, st);
        expect(f.capability().equals("ring") && f.namespaces().equals(java.util.Collections.singletonList("ring")), "capability and namespace");
        f.onReady(c);
        expect(f.onFrame(ring(1, false)) && s.starts == 1 && c.replies.get(0).equals("ok1"), "ring starts the sink and replies ok");
        expect(st.toJson().contains("\"ring\":\"ringing,alarm,no-dnd-access\""), "state reports ringing with the DND note");
        f.onFrame(ring(2, false));
        expect(s.starts == 1 && c.replies.get(1).equals("busy2"), "second ring is busy");
        f.onFrame(ring(3, true));
        expect(s.stops == 1 && c.replies.get(2).equals("ok3") && st.toJson().contains("\"ring\":\"idle\""), "stop:true stops and replies ok");
        f.onFrame(ring(4, true));
        expect(s.stops == 1 && c.replies.get(3).equals("ok4"), "stop while idle is ok and does not touch the sink");
        // User stop (Stop action or unlock) arrives through the callback, applied on the next tick.
        f.onFrame(ring(5, false));
        s.userStop.run();
        expect(c.wakes == 1, "user stop wakes the session");
        f.onTick(0);
        expect(s.stops == 2 && st.toJson().contains("\"ring\":\"idle\""), "tick applies the user stop");
        // Timeout.
        RingFeature quick = new RingFeature(s, st, 30);
        quick.onReady(c);
        quick.onFrame(ring(6, false));
        quick.onTick(0);
        expect(s.stops == 2, "not stopped before the cap");
        try { Thread.sleep(60); } catch (InterruptedException e) { return; }
        quick.onTick(0);
        expect(s.stops == 3, "cap stops the ring");
        expect(RingFeature.MAX_RING_MS == 60_000, "default cap is 60 s");
        // Session end stops a ring.
        f.onReady(c);
        f.onFrame(ring(7, false));
        f.onClosed();
        expect(s.stops == 4, "onClosed stops the ring");
        // Sink failure.
        s.fail = true;
        f.onReady(c = new Ctx());
        f.onFrame(ring(8, false));
        expect(c.replies.get(0).equals("internal8") && st.toJson().contains("\"ring\":\"failed\""), "sink failure replies internal");
        expect(!f.onFrame(new Envelope("ring.x", 9, 0, null)), "unknown type not handled");
        expect(DeviceInfo.displayName(" My Phone ", "SM-X").equals("My Phone") && DeviceInfo.displayName(null, "SM-X").equals("SM-X")
                && DeviceInfo.displayName("  ", "SM-X").equals("SM-X"), "device name falls back to the model");
        System.out.println("ring tests OK (" + checks + " checks)");
    }
}
