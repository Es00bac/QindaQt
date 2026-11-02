import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.FrameCodec;
import org.qindaqt.link.proto.Json;
import org.qindaqt.link.proto.Pairing;

/** Runs every case of tests/link/vectors/*.json against the Java codec. Exit 1 on any failure. */
public final class VectorTests {
    static int pass, fail;

    static void check(String name, boolean ok, String why) {
        if (ok) pass++; else { fail++; System.out.println("FAIL " + name + ": " + why); }
    }

    @SuppressWarnings("unchecked")
    static Map<String, Object> load(String dir, String f) throws Exception {
        return (Map<String, Object>) Json.parse(new String(Files.readAllBytes(Paths.get(dir, f)), "UTF-8"));
    }

    static byte[] hexBytes(String h) { return Pairing.unhex(h, h.length() / 2); }

    @SuppressWarnings("unchecked")
    public static void main(String[] a) throws Exception {
        String dir = a[0];
        Map<String, Object> fr = load(dir, "frames-v1.json");
        int dec = 0, str = 0, enc = 0;
        for (Object o : (List<Object>) fr.get("decode")) {
            Map<String, Object> c = (Map<String, Object>) o;
            String n = (String) c.get("name");
            Map<String, Object> ex = (Map<String, Object>) c.get("expect");
            byte[] bytes = hexBytes((String) c.get("hex"));
            FrameCodec d = new FrameCodec();
            d.feed(bytes, 0, bytes.length);
            FrameCodec.Result r = d.next();
            if (ex.containsKey("envelope")) {
                check(n, r.status == FrameCodec.Status.FRAME, "status " + r.status);
                if (r.status == FrameCodec.Status.FRAME) {
                    String got = Json.write(r.envelope.toObject());
                    String want = Json.write(norm((Map<String, Object>) ex.get("envelope")));
                    check(n, got.equals(want), got + " != " + want);
                }
            } else if (ex.containsKey("error")) {
                String e = ((String) ex.get("error")).toUpperCase().replace('-', '_');
                check(n, r.status.name().equals(e), r.status + " != " + e);
            } else {
                check(n, r.status == FrameCodec.Status.NEEDS_MORE, "status " + r.status);
            }
            dec++;
        }
        for (Object o : (List<Object>) fr.get("stream")) {
            Map<String, Object> c = (Map<String, Object>) o;
            byte[] bytes = hexBytes((String) c.get("hex"));
            // Feed one byte at a time: exercises partial-frame handling.
            FrameCodec d = new FrameCodec();
            List<Object> want = (List<Object>) ((Map<String, Object>) c.get("expect")).get("envelopes");
            int k = 0;
            for (byte b : bytes) {
                d.feed(new byte[] {b}, 0, 1);
                FrameCodec.Result r;
                while ((r = d.next()).status == FrameCodec.Status.FRAME) {
                    check((String) c.get("name"), k < want.size()
                            && Json.write(r.envelope.toObject()).equals(Json.write(norm((Map<String, Object>) want.get(k)))), "frame " + k);
                    k++;
                }
            }
            check((String) c.get("name"), k == want.size(), "count " + k);
            str++;
        }
        for (Object o : (List<Object>) fr.get("encode")) {
            Map<String, Object> c = (Map<String, Object>) o;
            Map<String, Object> env = norm((Map<String, Object>) c.get("envelope"));
            byte[] bytes = FrameCodec.encodeObject(env);
            FrameCodec d = new FrameCodec();
            d.feed(bytes, 0, bytes.length);
            FrameCodec.Result r = d.next();
            check((String) c.get("name"), r.status == FrameCodec.Status.FRAME
                    && Json.write(r.envelope.toObject()).equals(Json.write(env)), "roundtrip " + r.status);
            enc++;
        }
        Map<String, Object> pr = load(dir, "pairing-v1.json");
        int pc = 0;
        for (Object o : (List<Object>) pr.get("cases")) {
            Map<String, Object> c = (Map<String, Object>) o;
            String n = (String) c.get("name");
            byte[] sd = Pairing.unhex((String) c.get("spkiD"), 32), sp = Pairing.unhex((String) c.get("spkiP"), 32);
            byte[] nd = Pairing.unhex((String) c.get("nd"), 32), np = Pairing.unhex((String) c.get("np"), 32);
            check(n + "/commitment", Pairing.hex(Pairing.commitment(nd, sd, sp)).equals(c.get("commitment")), "commitment");
            check(n + "/verify", Pairing.verifyCommitment(Pairing.unhex((String) c.get("commitment"), 32), nd, sd, sp), "verify");
            check(n + "/code", Pairing.code(sd, sp, nd, np).equals(c.get("code")), "code");
            pc++;
        }
        System.out.println("frames decode=" + dec + " stream=" + str + " encode=" + enc + " pairing=" + pc
                + " checks pass=" + pass + " fail=" + fail);
        System.exit(fail == 0 ? 0 : 1);
    }

    /** Canonical envelope shape (what Envelope.toObject emits) for comparing vector expectations. */
    @SuppressWarnings("unchecked")
    static Map<String, Object> norm(Map<String, Object> m) {
        Envelope e = new Envelope((String) m.get("t"), m.containsKey("id") ? (Long) m.get("id") : 0,
                m.containsKey("re") ? (Long) m.get("re") : 0, (Map<String, Object>) m.get("b"));
        return e.toObject();
    }
}
