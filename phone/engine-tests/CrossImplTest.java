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
import java.util.Map;
import java.util.concurrent.TimeUnit;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.Link;
import org.qindaqt.link.engine.PinStore;
import org.qindaqt.link.engine.Session;
import org.qindaqt.link.engine.StatusSource;
import org.qindaqt.link.proto.Json;
import org.qindaqt.link.proto.Pairing;

/**
 * Cross-implementation check: the real Java Session over JSSE TLS 1.3 against the real C++
 * qindaqt-link-service on a private D-Bus session bus. Usage: CrossImplTest <qindaqt-link-service>.
 * Never touches the real state dir, port 47160, adb, notifications or the user's bus.
 * Accepting on the desktop uses Link1 ConfirmPairing on the private bus only (no portal prompt).
 */
public final class CrossImplTest {
    static Process svc;
    static String bus;

    static String run(long timeoutS, String... cmd) throws Exception {
        Process p = new ProcessBuilder(cmd).redirectErrorStream(true).start();
        String out = new String(p.getInputStream().readAllBytes(), StandardCharsets.UTF_8);
        if (!p.waitFor(timeoutS, TimeUnit.SECONDS)) { p.destroyForcibly(); throw new IOException("timeout " + cmd[0]); }
        return out;
    }

    static String gdbus(String method, String... args) throws Exception {
        java.util.List<String> l = new java.util.ArrayList<>(java.util.Arrays.asList(
                "gdbus", "call", "--address", bus, "--dest", "org.qindaqt.Link1", "--object-path"));
        l.add("/org/qindaqt/Link1"); l.add("--method"); l.add("org.qindaqt.Link1." + method);
        for (String a : args) l.add(a);
        return run(10, l.toArray(new String[0]));
    }

    static void expect(boolean ok, String what) {
        if (!ok) { System.out.println("FAIL " + what); cleanup(); System.exit(1); }
        System.out.println("ok   " + what);
    }

    static File pidFile;

    /** Kills only the processes this test started (the service pid is recorded by the wrapper). */
    static void cleanup() {
        try {
            if (pidFile != null && pidFile.exists())
                new ProcessBuilder("kill", new String(Files.readAllBytes(pidFile.toPath())).trim()).start().waitFor();
        } catch (Exception ignored) { /* best effort */ }
        if (svc != null) svc.destroy();
    }

    public static void main(String[] a) throws Exception {
        File tmp = Files.createTempDirectory("qlink-x").toFile();
        File addr = new File(tmp, "bus-address");
        pidFile = new File(addr.getPath() + ".pid");
        File data = new File(tmp, "desktop-data");
        data.mkdirs();
        File log = new File(tmp, "service.log");
        // dbus-run-session owns the private bus; the wrapper publishes its address then execs the service.
        ProcessBuilder pb = new ProcessBuilder("dbus-run-session", "--", "bash", "-c",
                "echo $$ > '" + addr + ".pid'; echo \"$DBUS_SESSION_BUS_ADDRESS\" > '" + addr + "'; exec \"$0\" --data-dir '" + data
                        + "' --port 0 --no-adb --no-notifications"
                        + " --trusted-caller \"$(readlink -f \"$(command -v gdbus)\")\"", a[0]);
        pb.environment().put("QT_QPA_PLATFORM", "offscreen");
        pb.redirectErrorStream(true).redirectOutput(log);
        svc = pb.start();
        long dl = System.currentTimeMillis() + 15000;
        while (!(addr.exists() && addr.length() > 0) && System.currentTimeMillis() < dl) Thread.sleep(100);
        expect(addr.exists() && addr.length() > 0, "private bus started");
        bus = new String(Files.readAllBytes(addr.toPath()), StandardCharsets.UTF_8).trim();
        int port = -1;
        for (dl = System.currentTimeMillis() + 15000; port < 0 && System.currentTimeMillis() < dl; Thread.sleep(200)) {
            Matcher m = Pattern.compile("uint16 (\\d+)").matcher(gdbus("ListenPort"));
            if (m.find()) port = Integer.parseInt(m.group(1));
        }
        expect(port > 0, "service up, ListenPort=" + port);

        Process monitor = new ProcessBuilder("gdbus", "monitor", "--address", bus, "--dest", "org.qindaqt.Link1")
                .redirectErrorStream(true).redirectOutput(new File(tmp, "monitor.log")).start();

        // Throwaway P-256 identity from the JDK's keytool.
        File p12 = new File(tmp, "phone.p12");
        run(30, "keytool", "-genkeypair", "-keystore", p12.getPath(), "-storetype", "PKCS12", "-storepass", "changeit",
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
        expect(tls.getSession().getProtocol().equals("TLSv1.3"), "TLS 1.3 handshake with the desktop");
        Link link = new Link() {
            public InputStream in() throws IOException { return tls.getInputStream(); }
            public OutputStream out() throws IOException { return tls.getOutputStream(); }
            public byte[] peerSpki() { return peer; }
            public byte[] ownSpki() { return own; }
            public void close() { try { tls.close(); } catch (IOException e) { /* ignore */ } }
        };
        StatusSource st = () -> SessionTest.m("battery", SessionTest.m("level", 42L, "charging", true, "plugged", "usb"),
                "network", SessionTest.m("type", "wifi", "signal", Json.NULL));
        EngineState state = new EngineState();
        PinStore pins = new PinStore(new File(tmp, "phone-pin"));
        Session s = new Session(link, pins, st, new DeviceInfo("xtest-phone", "xtest", "JVM", "0"), state);
        Thread t = new Thread(s::run);
        t.start();
        waitPhase(state, EngineState.Phase.CONNECTED, 10000);
        expect(true, "hello exchanged (phase connected)");
        s.command(Session.Command.PAIR);
        waitPhase(state, EngineState.Phase.CONFIRM, 10000);
        String id = Pairing.hex(own);
        Thread.sleep(300);
        String phoneCode = new JsonView(state.toJson()).get("code").replace(" ", "");
        String mon = new String(Files.readAllBytes(new File(tmp, "monitor.log").toPath()), StandardCharsets.UTF_8);
        Matcher cm = Pattern.compile("PairingRequested \\('[0-9a-f]+', '[^']*', '[^']*', '(\\d{6})'\\)").matcher(mon);
        String desktopCode = cm.find() ? cm.group(1) : "";
        expect(phoneCode.length() == 6 && phoneCode.equals(desktopCode), "pairing codes equal (6 digits each)");
        gdbus("ConfirmPairing", id, "true");
        s.command(Session.Command.ACCEPT);
        waitPhase(state, EngineState.Phase.READY, 10000);
        expect(Pairing.hex(peer).equals(pins.deviceId()), "phone pinned the desktop");
        String devices = "";
        boolean seen = false;
        for (dl = System.currentTimeMillis() + 10000; !seen && System.currentTimeMillis() < dl; Thread.sleep(300)) {
            devices = gdbus("Devices");
            seen = devices.contains(id) && devices.contains("\"ready\":true") && devices.contains("\"level\":42");
        }
        expect(seen, "Devices() lists the phone ready with status battery level 42");
        s.command(Session.Command.STOP);
        t.join(3000);
        monitor.destroy();
        cleanup();
        System.out.println("cross-implementation test OK");
        System.exit(0);
    }

    static void waitPhase(EngineState s, EngineState.Phase p, long ms) throws Exception {
        for (long dl = System.currentTimeMillis() + ms; System.currentTimeMillis() < dl; Thread.sleep(50))
            if (s.phase() == p) return;
        expect(false, "phase " + p + " not reached, at " + s.phase() + " " + s.toJson());
    }

    /** Minimal reader for EngineState JSON. */
    static final class JsonView {
        final Map<?, ?> m;
        JsonView(String j) throws Exception { m = (Map<?, ?>) Json.parse(j); }
        String get(String k) { return String.valueOf(m.get(k)); }
    }
}
