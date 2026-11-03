import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.cert.X509Certificate;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureRegistry;
import org.qindaqt.link.engine.Link;
import org.qindaqt.link.engine.PinStore;
import org.qindaqt.link.engine.Session;
import org.qindaqt.link.notify.NotificationFeature;
import org.qindaqt.link.notify.NotificationSource;
import org.qindaqt.link.notify.PhoneNotification;
import org.qindaqt.link.proto.Pairing;

/**
 * A JVM phone (real Session, real NotificationFeature, fake notification source) posts notifications to
 * the real qindaqt-link-service on a private D-Bus session bus. The service runs with --recording-notifier,
 * so nothing is sent to the user's notification server; the presenter records lengths and a SHA-256 of
 * the (synthetic) title, never text. Usage: CrossNotifyTest <qindaqt-link-service>.
 */
public final class CrossNotifyTest {
    static final String TITLE = "synthetic-title-1", TEXT = "synthetic-text-1";
    static final byte[] PNG = concat(new byte[] {(byte) 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'}, "fake-icon-bytes".getBytes(StandardCharsets.UTF_8));

    static byte[] concat(byte[] a, byte[] b) { byte[] r = Arrays.copyOf(a, a.length + b.length); System.arraycopy(b, 0, r, a.length, b.length); return r; }

    static String sha(byte[] b) throws Exception {
        StringBuilder sb = new StringBuilder();
        for (byte x : MessageDigest.getInstance("SHA-256").digest(b)) sb.append(String.format("%02x", x));
        return sb.toString();
    }

    static final class FakeSource implements NotificationSource {
        volatile Listener l;
        final List<String> dismissed = new ArrayList<>(), performed = new ArrayList<>();
        List<PhoneNotification> active = new ArrayList<>();
        String iconHash;
        public void setListener(Listener x) { l = x; }
        public boolean accessGranted() { return true; }
        public List<PhoneNotification> active() { return active; }
        public boolean dismiss(String key) { dismissed.add(key); return true; }
        public String perform(String key, String action, String title, String text) { performed.add(key + "/" + action + "/" + (text == null ? "-" : text)); return null; }
        public byte[] icon(String hash) { return hash.equals(iconHash) ? PNG : null; }
    }

    static String gdbusNotif(String method, String... args) throws Exception {
        List<String> l = new ArrayList<>(Arrays.asList("gdbus", "call", "--address", CrossImplTest.bus, "--dest", "org.qindaqt.Link1",
                "--object-path", "/org/qindaqt/Link1", "--method", "org.qindaqt.Link1.Notifications." + method));
        l.addAll(Arrays.asList(args));
        return CrossImplTest.run(15, l.toArray(new String[0]));
    }

    static String rec(File f) {
        try { return f.exists() ? new String(Files.readAllBytes(f.toPath()), StandardCharsets.UTF_8) : ""; } catch (IOException e) { return ""; }
    }

    static boolean waitFor(java.util.function.BooleanSupplier c, long ms) throws Exception {
        for (long dl = System.currentTimeMillis() + ms; System.currentTimeMillis() < dl; Thread.sleep(50)) if (c.getAsBoolean()) return true;
        return c.getAsBoolean();
    }

    static int count(String s, String needle) { int n = 0; for (int i = s.indexOf(needle); i >= 0; i = s.indexOf(needle, i + 1)) n++; return n; }

    static PhoneNotification note(String key, String title, String text, String icon) {
        return new PhoneNotification(key, "synthetic.pkg", "Synthetic", title, text, 1767225600000L, icon,
                Arrays.asList(new PhoneNotification.Action("0", "Archive", false), new PhoneNotification.Action("1", "Reply", true)));
    }

    public static void main(String[] a) throws Exception {
        File tmp = Files.createTempDirectory("qlink-n").toFile();
        File addr = new File(tmp, "bus-address");
        CrossImplTest.pidFile = new File(addr.getPath() + ".pid");
        File data = new File(tmp, "data"), cfg = new File(tmp, "config"), cache = new File(tmp, "cache");
        File recFile = new File(tmp, "recording.jsonl"), log = new File(tmp, "service.log");
        ProcessBuilder pb = new ProcessBuilder("dbus-run-session", "--", "bash", "-c",
                "echo $$ > '" + addr + ".pid'; echo \"$DBUS_SESSION_BUS_ADDRESS\" > '" + addr + "'; exec \"$0\" --data-dir '" + data
                        + "' --config-dir '" + cfg + "' --cache-dir '" + cache + "' --port 0 --no-adb --no-notifications --recording-notifier '" + recFile + "'", a[0]);
        pb.environment().put("QT_QPA_PLATFORM", "offscreen");
        pb.redirectErrorStream(true).redirectOutput(log);
        CrossImplTest.svc = pb.start();
        long dl = System.currentTimeMillis() + 15000;
        while (!(addr.exists() && addr.length() > 0) && System.currentTimeMillis() < dl) Thread.sleep(100);
        CrossImplTest.expect(addr.exists() && addr.length() > 0, "private bus started");
        CrossImplTest.bus = new String(Files.readAllBytes(addr.toPath()), StandardCharsets.UTF_8).trim();
        int port = -1;
        for (dl = System.currentTimeMillis() + 15000; port < 0 && System.currentTimeMillis() < dl; Thread.sleep(200)) {
            Matcher m = Pattern.compile("uint16 (\\d+)").matcher(CrossImplTest.gdbus("ListenPort"));
            if (m.find()) port = Integer.parseInt(m.group(1));
        }
        CrossImplTest.expect(port > 0, "service up, ListenPort=" + port);
        Process monitor = new ProcessBuilder("gdbus", "monitor", "--address", CrossImplTest.bus, "--dest", "org.qindaqt.Link1")
                .redirectErrorStream(true).redirectOutput(new File(tmp, "monitor.log")).start();

        File p12 = new File(tmp, "phone.p12");
        CrossImplTest.run(30, "keytool", "-genkeypair", "-keystore", p12.getPath(), "-storetype", "PKCS12", "-storepass", "changeit",
                "-keypass", "changeit", "-alias", "p", "-keyalg", "EC", "-groupname", "secp256r1", "-dname", "CN=test-phone", "-validity", "30");
        KeyStore ks = KeyStore.getInstance("PKCS12");
        try (InputStream in = Files.newInputStream(p12.toPath())) { ks.load(in, "changeit".toCharArray()); }
        final byte[] own = MessageDigest.getInstance("SHA-256").digest(((X509Certificate) ks.getCertificate("p")).getPublicKey().getEncoded());
        KeyManagerFactory kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        kmf.init(ks, "changeit".toCharArray());
        SSLContext ctx = SSLContext.getInstance("TLS");
        ctx.init(kmf.getKeyManagers(), new TrustManager[] {new X509TrustManager() {
            public void checkClientTrusted(X509Certificate[] c, String t) { }
            public void checkServerTrusted(X509Certificate[] c, String t) { }
            public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
        }}, null);
        Socket raw = new Socket(InetAddress.getLoopbackAddress(), port);
        final SSLSocket tls = (SSLSocket) ctx.getSocketFactory().createSocket(raw, "127.0.0.1", port, true);
        tls.setEnabledProtocols(new String[] {"TLSv1.3"});
        tls.startHandshake();
        final byte[] peer = MessageDigest.getInstance("SHA-256").digest(tls.getSession().getPeerCertificates()[0].getPublicKey().getEncoded());
        Link link = new Link() {
            public InputStream in() throws IOException { return tls.getInputStream(); }
            public OutputStream out() throws IOException { return tls.getOutputStream(); }
            public byte[] peerSpki() { return peer; }
            public byte[] ownSpki() { return own; }
            public void close() { try { tls.close(); } catch (IOException e) { /* ignore */ } }
        };

        FakeSource src = new FakeSource();
        src.iconHash = sha(PNG);
        src.active.add(note("snap-1", TITLE, TEXT, src.iconHash));   // present before READY: arrives in the snapshot
        EngineState state = new EngineState();
        PinStore pins = new PinStore(new File(tmp, "phone-pin"));
        Session s = new Session(link, pins, new FeatureRegistry().add(new NotificationFeature(src, state)),
                new DeviceInfo("xtest-phone", "xtest", "JVM", "0"), state);
        Thread t = new Thread(s::run);
        t.start();
        CrossImplTest.waitPhase(state, EngineState.Phase.CONNECTED, 10000);
        s.command(Session.Command.PAIR);
        CrossImplTest.waitPhase(state, EngineState.Phase.CONFIRM, 10000);
        String id = Pairing.hex(own);
        CrossImplTest.gdbus("ConfirmPairing", id, "true");
        s.command(Session.Command.ACCEPT);
        CrossImplTest.waitPhase(state, EngineState.Phase.READY, 10000);

        // 1. snapshot item reaches the presenter, the icon is fetched, verified and cached before showing
        CrossImplTest.expect(waitFor(() -> count(rec(recFile), "\"op\":\"present\"") >= 1, 10000), "snapshot item presented on the desktop");
        String r = rec(recFile);
        CrossImplTest.expect(r.contains("\"app\":\"Synthetic (phone)\"") && r.contains("\"summarySha256\":\"" + sha(TITLE.getBytes(StandardCharsets.UTF_8)) + "\""), "app label and title arrived intact (checked by hash)");
        CrossImplTest.expect(r.contains("\"hasIcon\":true") && new File(cache, "icons/" + src.iconHash + ".png").exists(), "icon fetched over the link and cached by hash");
        CrossImplTest.expect(r.contains("\"quiet\":true"), "snapshot items are presented quietly");

        // 2. new post, then update replaces in place
        src.l.onPosted(note("k-2", "second-title", "body", src.iconHash));
        CrossImplTest.expect(waitFor(() -> count(rec(recFile), "\"op\":\"present\"") >= 2, 10000), "live post presented");
        src.l.onPosted(note("k-2", "second-title", "changed body", src.iconHash));
        CrossImplTest.expect(waitFor(() -> count(rec(recFile), "\"op\":\"present\"") >= 3 && rec(recFile).contains("\"replaces\":2"), 10000), "update replaces the earlier notification");

        // 3. dismissal on the desktop cancels on the phone
        Files.write(new File(recFile.getPath() + ".cmd").toPath(), "dismiss 1\n".getBytes(StandardCharsets.UTF_8));
        CrossImplTest.expect(waitFor(() -> src.dismissed.contains("snap-1"), 10000), "desktop dismissal reached the phone");

        // 4. action button and Reply() over Link1.Notifications
        Files.write(new File(recFile.getPath() + ".cmd").toPath(), "action 2 a:0\n".getBytes(StandardCharsets.UTF_8));
        CrossImplTest.expect(waitFor(() -> src.performed.contains("k-2/0/-"), 10000), "desktop action button reached the phone");
        String reply = gdbusNotif("Reply", id, "k-2", "1", "synthetic reply");
        CrossImplTest.expect(reply.contains("('',)") && waitFor(() -> src.performed.contains("k-2/1/synthetic reply"), 10000), "Reply() delivered text to the phone, got " + reply.trim());
        CrossImplTest.expect(waitFor(() -> rec(new File(tmp, "monitor.log")).contains("ReplyFinished"), 10000), "ReplyFinished signalled the phone's answer");
        CrossImplTest.expect(gdbusNotif("Reply", id, "k-2", "9", "x").contains("not-found"), "Reply() with an unknown action reports not-found");

        // 5. removal on the phone closes on the desktop
        src.l.onRemoved("k-2");
        CrossImplTest.expect(waitFor(() -> rec(recFile).contains("\"handle\":2,\"op\":\"close\""), 10000), "phone removal closed the desktop notification");

        // 6. apps and mute
        String apps = gdbusNotif("ListApps");
        CrossImplTest.expect(apps.contains("synthetic.pkg") && apps.contains("\"muted\":false"), "ListApps lists the phone app");
        gdbusNotif("SetAppMuted", "synthetic.pkg", "true");
        src.l.onPosted(note("k-3", "muted-title", "x", src.iconHash));
        Thread.sleep(600);
        CrossImplTest.expect(count(rec(recFile), "\"op\":\"present\"") == 3, "muted app is not presented");
        CrossImplTest.expect(new String(Files.readAllBytes(new File(cfg, "notifications.json").toPath()), StandardCharsets.UTF_8).contains("synthetic.pkg"), "mute list persisted");
        String intro = CrossImplTest.run(15, "gdbus", "introspect", "--address", CrossImplTest.bus, "--dest", "org.qindaqt.Link1", "--object-path", "/org/qindaqt/Link1");
        CrossImplTest.expect(intro.contains("org.qindaqt.Link1.Notifications") && intro.contains("CurrentChanged"), "Notifications interface is introspectable");

        // 7. no notification text reaches the service log
        String svcLog = rec(log);
        CrossImplTest.expect(!svcLog.contains(TITLE) && !svcLog.contains(TEXT) && !svcLog.contains("synthetic reply"), "service log carries no notification content");

        s.command(Session.Command.STOP);
        t.join(3000);
        monitor.destroy();
        CrossImplTest.cleanup();
        System.out.println("cross-implementation notification test OK");
        System.exit(0);
    }
}
