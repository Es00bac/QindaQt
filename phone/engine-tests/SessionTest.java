import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Files;
import java.security.SecureRandom;
import java.util.LinkedHashMap;
import java.util.Map;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.Link;
import org.qindaqt.link.engine.PinStore;
import org.qindaqt.link.engine.Session;
import org.qindaqt.link.engine.StatusSource;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.FrameCodec;
import org.qindaqt.link.proto.Pairing;

/** Drives a phone Session against a scripted in-JVM desktop over loopback TCP: pair, status, ring. */
public final class SessionTest {
    static final byte[] SPKI_D = new byte[32], SPKI_P = new byte[32];

    static Envelope read(InputStream in, FrameCodec dec) throws IOException {
        byte[] b = new byte[4096];
        while (true) {
            FrameCodec.Result r = dec.next();
            if (r.status == FrameCodec.Status.FRAME) return r.envelope;
            if (r.status != FrameCodec.Status.NEEDS_MORE) throw new IOException("" + r.status);
            int n = in.read(b);
            if (n < 0) throw new IOException("eof");
            dec.feed(b, 0, n);
        }
    }

    static void write(OutputStream o, Envelope e) throws IOException { o.write(FrameCodec.encode(e)); o.flush(); }

    static Map<String, Object> m(Object... kv) {
        Map<String, Object> r = new LinkedHashMap<>();
        for (int i = 0; i < kv.length; i += 2) r.put((String) kv[i], kv[i + 1]);
        return r;
    }

    static void expect(boolean ok, String what) {
        if (!ok) { System.out.println("FAIL " + what); System.exit(1); }
    }

    public static void main(String[] a) throws Exception {
        new SecureRandom().nextBytes(SPKI_D);
        new SecureRandom().nextBytes(SPKI_P);
        File dir = Files.createTempDirectory("qlink").toFile();
        PinStore pins = new PinStore(new File(dir, "pin"));
        ServerSocket ss = new ServerSocket(0, 1, java.net.InetAddress.getLoopbackAddress());
        final Socket phoneSide = new Socket(java.net.InetAddress.getLoopbackAddress(), ss.getLocalPort());
        final Socket deskSide = ss.accept();
        Link link = new Link() {
            public InputStream in() throws IOException { return phoneSide.getInputStream(); }
            public OutputStream out() throws IOException { return phoneSide.getOutputStream(); }
            public byte[] peerSpki() { return SPKI_D; }
            public byte[] ownSpki() { return SPKI_P; }
            public void close() { try { phoneSide.close(); } catch (IOException e) { /* ignore */ } }
        };
        StatusSource st = () -> m("battery", m("level", 55L, "charging", false, "plugged", "none"),
                "network", m("type", "wifi", "signal", org.qindaqt.link.proto.Json.NULL));
        EngineState state = new EngineState();
        final Session s = new Session(link, pins, st, new DeviceInfo("test", "T", "Android", "0"), state);
        Thread t = new Thread(s::run);
        t.start();

        InputStream in = deskSide.getInputStream();
        OutputStream out = deskSide.getOutputStream();
        FrameCodec dec = new FrameCodec();
        Envelope hello = read(in, dec);
        expect(hello.t.equals("hello") && Boolean.FALSE.equals(hello.b.get("trusted")), "phone hello untrusted");
        expect(!((java.util.List<?>) hello.b.get("caps")).contains("ring"), "ring not advertised");
        write(out, Envelope.of("hello", m("proto", 1L, "role", "desktop", "name", "fake", "model", "m", "os", "o",
                "app", "0", "caps", java.util.Arrays.asList("status", "ring"), "trusted", false)));
        while (state.phase() != EngineState.Phase.CONNECTED) Thread.sleep(20);
        s.command(Session.Command.PAIR);
        Envelope req = read(in, dec);
        expect(req.t.equals("pair.request") && req.id != 0, "pair.request");
        byte[] nd = new byte[32];
        new SecureRandom().nextBytes(nd);
        write(out, new Envelope("pair.commit", 0, req.id, m("c", Pairing.hex(Pairing.commitment(nd, SPKI_D, SPKI_P)))));
        Envelope nonce = read(in, dec);
        expect(nonce.t.equals("pair.nonce"), "pair.nonce");
        byte[] np = Pairing.unhex((String) nonce.b.get("np"), 32);
        write(out, Envelope.of("pair.reveal", m("nd", Pairing.hex(nd))));
        while (state.phase() != EngineState.Phase.CONFIRM) Thread.sleep(20);
        String want = Pairing.display(Pairing.code(SPKI_D, SPKI_P, nd, np));
        expect(state.toJson().contains("\"code\":\"" + want + "\""), "code shown equals desktop code");
        s.command(Session.Command.ACCEPT);
        Envelope conf = read(in, dec);
        expect(conf.t.equals("pair.confirm") && Boolean.TRUE.equals(conf.b.get("accept")), "phone confirm");
        write(out, Envelope.of("pair.confirm", m("accept", true)));
        Envelope status = read(in, dec);
        expect(status.t.equals("status"), "status after pairing");
        expect(Pairing.hex(SPKI_D).equals(pins.deviceId()), "pin stored");
        write(out, new Envelope("ring", 7, 0, null));
        Envelope err = read(in, dec);
        expect(err.t.equals("error") && err.re == 7 && "unsupported".equals(err.b.get("code")), "ring unsupported");
        write(out, new Envelope("ping", 9, 0, null));
        Envelope pong = read(in, dec);
        expect(pong.t.equals("pong") && pong.re == 9, "pong");
        s.command(Session.Command.STOP);
        t.join(3000);
        expect(!t.isAlive(), "session ended");
        System.out.println("session loopback OK code=" + want.length() + "chars phase=" + state.phase());
    }
}
