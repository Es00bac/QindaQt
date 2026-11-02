import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Files;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.CopyOnWriteArrayList;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.engine.FeatureHandler;
import org.qindaqt.link.engine.FeatureRegistry;
import org.qindaqt.link.engine.Link;
import org.qindaqt.link.engine.PinStore;
import org.qindaqt.link.engine.Session;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.FrameCodec;
import org.qindaqt.link.proto.Pairing;

/** Feature plug-in contract: caps, READY-only routing, unknown-type, capability gating, onClosed. */
public final class FeatureTest {
    static final byte[] D = new byte[32], P = new byte[32];

    static final class Fake implements FeatureHandler {
        final List<String> seen = new CopyOnWriteArrayList<>();
        volatile int closed, readied;
        volatile boolean sentOwn, sentForeign = true;
        FeatureContext ctx;
        public String capability() { return "fake"; }
        public List<String> namespaces() { return Collections.singletonList("fake"); }
        public void onReady(FeatureContext c) throws IOException {
            ctx = c;
            readied++;
            sentOwn = c.send(Envelope.of("fake.hello", SessionTest.m()));
            sentForeign = c.send(Envelope.of("status", SessionTest.m()));
        }
        public boolean onFrame(Envelope e) throws IOException {
            seen.add(e.t);
            if (e.t.equals("fake.echo")) { ctx.replyOk(e.id); return true; }
            return false;
        }
        public void onClosed() { closed++; }
    }

    static Envelope read(InputStream in, FrameCodec d) throws IOException { return SessionTest.read(in, d); }

    /** Starts a session whose pin matches D; peerCaps and trusted come from the scripted desktop hello. */
    static Object[] start(Fake f, boolean pinned, List<String> peerCaps, File dir, String pinName) throws Exception {
        PinStore pins = new PinStore(new File(dir, pinName));
        if (pinned) pins.set(Pairing.hex(D), "d");
        ServerSocket ss = new ServerSocket(0, 1, InetAddress.getLoopbackAddress());
        final Socket ps = new Socket(InetAddress.getLoopbackAddress(), ss.getLocalPort());
        Socket ds = ss.accept();
        ss.close();
        Link link = new Link() {
            public InputStream in() throws IOException { return ps.getInputStream(); }
            public OutputStream out() throws IOException { return ps.getOutputStream(); }
            public byte[] peerSpki() { return D; }
            public byte[] ownSpki() { return P; }
            public void close() { try { ps.close(); } catch (IOException e) { /* ignore */ } }
        };
        EngineState st = new EngineState();
        Session s = new Session(link, pins, new FeatureRegistry().add(f), new DeviceInfo("t", "t", "a", "0"), st);
        Thread t = new Thread(s::run);
        t.start();
        FrameCodec dec = new FrameCodec();
        Envelope hello = read(ds.getInputStream(), dec);
        SessionTest.write(ds.getOutputStream(), Envelope.of("hello", SessionTest.m("proto", 1L, "role", "desktop", "name", "x",
                "model", "m", "os", "o", "app", "0", "caps", new ArrayList<Object>(peerCaps), "trusted", pinned)));
        return new Object[] {s, t, ds, dec, hello, st};
    }

    @SuppressWarnings("unchecked")
    public static void main(String[] a) throws Exception {
        new SecureRandom().nextBytes(D);
        new SecureRandom().nextBytes(P);
        File dir = Files.createTempDirectory("qlink-f").toFile();

        // A. READY via pin; caps advertised; routing; unknown-type; onClosed.
        Fake f = new Fake();
        Object[] r = start(f, true, Arrays.asList("fake", "status"), dir, "pinA");
        Envelope hello = (Envelope) r[4];
        SessionTest.expect(hello.b.get("caps").equals(Arrays.asList("fake")), "hello.caps lists the registered capability: " + hello.b.get("caps"));
        Socket ds = (Socket) r[2];
        InputStream in = ds.getInputStream();
        OutputStream out = ds.getOutputStream();
        FrameCodec dec = (FrameCodec) r[3];
        Envelope own = read(in, dec);
        SessionTest.expect(own.t.equals("fake.hello"), "feature sent its frame after READY");
        SessionTest.expect(f.sentOwn && !f.sentForeign, "send gated: own namespace ok, foreign namespace refused");
        SessionTest.write(out, new Envelope("fake.echo", 3, 0, null));
        Envelope ok = read(in, dec);
        SessionTest.expect(ok.t.equals("ok") && ok.re == 3, "handled request answered via context");
        SessionTest.write(out, new Envelope("fake.nope", 4, 0, null));
        Envelope err = read(in, dec);
        SessionTest.expect(err.t.equals("error") && err.re == 4 && "unknown-type".equals(err.b.get("code")), "unhandled request -> unknown-type");
        SessionTest.write(out, new Envelope("zzz.what", 5, 0, null));
        err = read(in, dec);
        SessionTest.expect(err.re == 5 && "unknown-type".equals(err.b.get("code")), "unregistered namespace -> unknown-type");
        SessionTest.write(out, Envelope.of("zzz.quiet", null));
        SessionTest.write(out, new Envelope("ping", 6, 0, null));
        SessionTest.expect(read(in, dec).t.equals("pong"), "unknown frame without id ignored");
        SessionTest.expect(f.seen.equals(Arrays.asList("fake.echo", "fake.nope")), "handler saw only its namespace: " + f.seen);
        ((Session) r[0]).command(Session.Command.STOP);
        ((Thread) r[1]).join(3000);
        SessionTest.expect(f.closed == 1 && f.readied == 1, "onClosed called once on disconnect");

        // B. Peer lacks the capability: nothing is sent.
        f = new Fake();
        r = start(f, true, Arrays.asList("status"), dir, "pinB");
        ds = (Socket) r[2];
        in = ds.getInputStream();
        out = ds.getOutputStream();
        dec = (FrameCodec) r[3];
        SessionTest.write(out, new Envelope("ping", 1, 0, null));
        Envelope first = read(in, dec);
        SessionTest.expect(first.t.equals("pong"), "no feature frame precedes the pong; peer lacks cap");
        SessionTest.expect(!f.sentOwn, "send refused when the peer did not advertise the capability");
        ((Session) r[0]).command(Session.Command.STOP);
        ((Thread) r[1]).join(3000);
        SessionTest.expect(f.closed == 1, "onClosed after stop");

        // C. Unpaired: a feature frame closes the connection and never reaches the handler; no onReady.
        f = new Fake();
        r = start(f, false, Arrays.asList("fake"), dir, "pinC");
        ds = (Socket) r[2];
        SessionTest.write(ds.getOutputStream(), Envelope.of("fake.echo", null));
        ((Thread) r[1]).join(5000);
        SessionTest.expect(!((Thread) r[1]).isAlive(), "unpaired feature frame ends the session");
        SessionTest.expect(f.seen.isEmpty() && f.readied == 0 && f.closed == 0, "handler untouched before READY");
        System.out.println("feature tests OK");
        System.exit(0);
    }
}
