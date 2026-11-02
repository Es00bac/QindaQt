import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.file.Files;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.Locale;
import java.util.Map;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.Link;
import org.qindaqt.link.engine.PinStore;
import org.qindaqt.link.engine.Session;
import org.qindaqt.link.engine.StatusSource;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.FrameCodec;
import org.qindaqt.link.proto.Json;
import org.qindaqt.link.proto.Pairing;

/** Repair-round regressions: locale, strict hex, pin persistence failure, bounded stalled writes. */
public final class RepairTests {
    static final byte[] D = new byte[32], P = new byte[32];

    static final class Rig {
        Socket phoneSide, deskSide;
        Session s;
        Thread t;
        EngineState state = new EngineState();
        InputStream in;
        OutputStream out;
        FrameCodec dec = new FrameCodec();

        Rig(PinStore pins) throws Exception {
            ServerSocket ss = new ServerSocket(0, 1, InetAddress.getLoopbackAddress());
            phoneSide = new Socket(InetAddress.getLoopbackAddress(), ss.getLocalPort());
            deskSide = ss.accept();
            ss.close();
            final Socket ps = phoneSide;
            Link link = new Link() {
                public InputStream in() throws IOException { return ps.getInputStream(); }
                public OutputStream out() throws IOException { return ps.getOutputStream(); }
                public byte[] peerSpki() { return D; }
                public byte[] ownSpki() { return P; }
                public void close() { try { ps.close(); } catch (IOException e) { /* ignore */ } }
            };
            StatusSource st = () -> SessionTest.m("battery", SessionTest.m("level", 5L, "charging", false, "plugged", "none"),
                    "network", SessionTest.m("type", "none", "signal", Json.NULL));
            s = new Session(link, pins, st, new DeviceInfo("t", "t", "a", "0"), state);
            t = new Thread(s::run);
            t.start();
            in = deskSide.getInputStream();
            out = deskSide.getOutputStream();
            SessionTest.read(in, dec); // phone hello
            SessionTest.write(out, Envelope.of("hello", SessionTest.m("proto", 1L, "role", "desktop", "name", "x", "model", "m",
                    "os", "o", "app", "0", "caps", Arrays.asList("status"), "trusted", false)));
            while (state.phase() != EngineState.Phase.CONNECTED) Thread.sleep(10);
            s.command(Session.Command.PAIR);
        }

        Envelope request() throws Exception { return SessionTest.read(in, dec); }
    }

    static String lastBye(Rig r) throws Exception {
        while (true) {
            Envelope e = SessionTest.read(r.in, r.dec);
            if (e.t.equals("bye")) return (String) e.b.get("reason");
        }
    }

    public static void main(String[] a) throws Exception {
        new SecureRandom().nextBytes(D);
        new SecureRandom().nextBytes(P);
        File dir = Files.createTempDirectory("qlink-r").toFile();

        // 1. Locale: digits stay ASCII under an Arabic default locale.
        Locale old = Locale.getDefault();
        Locale.setDefault(Locale.forLanguageTag("ar-SA"));
        String code = Pairing.code(D, P, new byte[32], new byte[32]);
        Locale.setDefault(old);
        SessionTest.expect(code.matches("[0-9]{6}"), "ASCII code under ar locale: " + code);
        SessionTest.expect(Pairing.display(code).matches("[0-9]{3} [0-9]{3}"), "display form");
        Locale.setDefault(Locale.forLanguageTag("ar-SA"));
        String esc = Json.write("\u0001");
        Locale.setDefault(old);
        SessionTest.expect(esc.equals("\"\\u0001\""), "json escape ASCII under ar locale");

        // 2. Strict hex helper.
        SessionTest.expect(Pairing.unhex("A".repeat(64), 32) == null, "uppercase hex rejected");
        SessionTest.expect(Pairing.unhex("\u0661".repeat(64), 32) == null, "non-ASCII digit rejected");
        SessionTest.expect(Pairing.unhex("a".repeat(63), 32) == null, "short hex rejected");
        SessionTest.expect(Pairing.unhex("a".repeat(64), 32) != null, "valid hex accepted");

        // 3. Uppercase commitment -> bye{pair-rejected}.
        Rig r = new Rig(new PinStore(new File(dir, "pin1")));
        Envelope req = r.request();
        SessionTest.expect(req.t.equals("pair.request"), "request");
        byte[] c = Pairing.commitment(new byte[32], D, P);
        SessionTest.write(r.out, new Envelope("pair.commit", 0, req.id, SessionTest.m("c", Pairing.hex(c).toUpperCase())));
        SessionTest.expect("pair-rejected".equals(lastBye(r)), "uppercase commit -> pair-rejected");
        r.t.join(3000);
        SessionTest.expect(!r.t.isAlive(), "session ended after malformed commit");

        // 4. Uppercase nd in reveal -> pair-rejected.
        r = new Rig(new PinStore(new File(dir, "pin2")));
        req = r.request();
        byte[] nd = new byte[32];
        SessionTest.write(r.out, new Envelope("pair.commit", 0, req.id, SessionTest.m("c", Pairing.hex(Pairing.commitment(nd, D, P)))));
        SessionTest.expect(r.request().t.equals("pair.nonce"), "nonce");
        SessionTest.write(r.out, Envelope.of("pair.reveal", SessionTest.m("nd", "0".repeat(63) + "F")));
        SessionTest.expect("pair-rejected".equals(lastBye(r)), "bad reveal -> pair-rejected");

        // 5. Pin cannot be persisted -> bye{error}, never READY, no pin.
        PinStore bad = new PinStore(new File(new File(dir, "missing-dir"), "pin"));
        r = new Rig(bad);
        req = r.request();
        SessionTest.write(r.out, new Envelope("pair.commit", 0, req.id, SessionTest.m("c", Pairing.hex(Pairing.commitment(nd, D, P)))));
        r.request();
        SessionTest.write(r.out, Envelope.of("pair.reveal", SessionTest.m("nd", Pairing.hex(nd))));
        while (r.state.phase() != EngineState.Phase.CONFIRM) Thread.sleep(10);
        r.s.command(Session.Command.ACCEPT);
        SessionTest.write(r.out, Envelope.of("pair.confirm", SessionTest.m("accept", true)));
        String reason = lastBye(r);
        SessionTest.expect("error".equals(reason), "pin failure -> bye error, got " + reason);
        SessionTest.expect(r.state.phase() == EngineState.Phase.ERROR, "state ERROR not READY");
        SessionTest.expect(bad.deviceId() == null, "no pin recorded");

        // 6. A peer that never reads: pongs back up, the bounded outbox / write watchdog ends the session
        //    instead of growing memory without limit or hanging past the dead timer.
        r = new Rig(new PinStore(new File(dir, "pin4")));
        byte[] ping = FrameCodec.encode(new Envelope("ping", 5, 0, null));
        long t0 = System.currentTimeMillis();
        try {
            for (int i = 0; i < 3_000_000 && r.t.isAlive(); i++) { r.out.write(ping); }
        } catch (IOException e) { /* phone closed on us: expected */ }
        r.t.join(30000);
        SessionTest.expect(!r.t.isAlive(), "stalled peer ends the session");
        System.out.println("stalled-peer session ended after " + (System.currentTimeMillis() - t0) + " ms");

        System.out.println("repair tests OK");
        System.exit(0);
    }
}
