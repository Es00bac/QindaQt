import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.cert.X509Certificate;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocket;
import javax.net.ssl.TrustManager;
import javax.net.ssl.X509TrustManager;
import org.qindaqt.link.engine.Backoff;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.EndpointConnector;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureRegistry;
import org.qindaqt.link.engine.LanMemory;
import org.qindaqt.link.engine.Link;
import org.qindaqt.link.engine.LinkEngine;
import org.qindaqt.link.engine.PinStore;
import org.qindaqt.link.engine.Session;
import org.qindaqt.link.engine.StatusFeature;
import org.qindaqt.link.engine.StatusSource;
import org.qindaqt.link.proto.Json;
import org.qindaqt.link.proto.Pairing;

/**
 * Cross-implementation LAN check: the real phone engine (EndpointConnector + LinkEngine over JSSE TLS 1.3)
 * against the real qindaqt-link-service with LAN enabled over a 127.0.0.2 stand-in network, on a private
 * D-Bus session bus. Pairing is refused until the pairing window is opened, then completes and Devices()
 * reports transport "lan". Usage: CrossLanTest <qindaqt-link-service>.
 *
 * AGENT-GUARD: the opener refuses the USB endpoint without touching the network, because 127.0.0.1:47160 on a
 * developer machine may be the user's real service. Only the private service on 127.0.0.2 is contacted.
 */
public final class CrossLanTest {
    static void expect(boolean ok, String what) { CrossImplTest.expect(ok, what); }

    public static void main(String[] a) throws Exception {
        File tmp = Files.createTempDirectory("qlink-xl").toFile();
        File addr = new File(tmp, "bus-address");
        CrossImplTest.pidFile = new File(addr.getPath() + ".pid");
        File data = new File(tmp, "desktop-data"), cfg = new File(tmp, "desktop-config");
        data.mkdirs();
        int lanPort;
        try (ServerSocket probe = new ServerSocket()) {
            probe.bind(new InetSocketAddress("127.0.0.2", 0));
            lanPort = probe.getLocalPort();
        }
        ProcessBuilder pb = new ProcessBuilder("dbus-run-session", "--", "bash", "-c",
                "echo $$ > '" + addr + ".pid'; echo \"$DBUS_SESSION_BUS_ADDRESS\" > '" + addr + "'; exec \"$0\" --data-dir '" + data
                        + "' --config-dir '" + cfg + "' --port 0 --lan-port " + lanPort + " --lan-test-address 127.0.0.2"
                        + " --pairing-window-ms 60000 --no-adb --no-avahi --no-notifications --no-input"
                        + " --trusted-caller \"$(readlink -f \"$(command -v gdbus)\")\"", a[0]);
        pb.environment().put("QT_QPA_PLATFORM", "offscreen");
        pb.redirectErrorStream(true).redirectOutput(new File(tmp, "service.log"));
        CrossImplTest.svc = pb.start();
        long dl = System.currentTimeMillis() + 15000;
        while (!(addr.exists() && addr.length() > 0) && System.currentTimeMillis() < dl) Thread.sleep(100);
        expect(addr.exists() && addr.length() > 0, "private bus started");
        CrossImplTest.bus = new String(Files.readAllBytes(addr.toPath()), StandardCharsets.UTF_8).trim();
        boolean up = false;
        for (dl = System.currentTimeMillis() + 15000; !up && System.currentTimeMillis() < dl; Thread.sleep(200)) {
            up = CrossImplTest.gdbus("ListenPort").contains("uint16");
        }
        expect(up, "service up");
        expect(CrossImplTest.run(10, "gdbus", "call", "--address", CrossImplTest.bus, "--dest", "org.qindaqt.Link1", "--object-path",
                "/org/qindaqt/Link1", "--method", "org.freedesktop.DBus.Properties.Get", "org.qindaqt.Link1", "LanEnabled").contains("false"),
                "LAN is off by default");
        Process monitor = new ProcessBuilder("gdbus", "monitor", "--address", CrossImplTest.bus, "--dest", "org.qindaqt.Link1")
                .redirectErrorStream(true).redirectOutput(new File(tmp, "monitor.log")).start();
        CrossImplTest.gdbus("SetLanEnabled", "true");
        Thread.sleep(300);
        expect(CrossImplTest.run(10, "gdbus", "call", "--address", CrossImplTest.bus, "--dest", "org.qindaqt.Link1", "--object-path",
                "/org/qindaqt/Link1", "--method", "org.freedesktop.DBus.Properties.Get", "org.qindaqt.Link1", "LanStatus").contains("listening"),
                "LAN listening after SetLanEnabled(true)");

        // Throwaway P-256 identity.
        File p12 = new File(tmp, "phone.p12");
        CrossImplTest.run(30, "keytool", "-genkeypair", "-keystore", p12.getPath(), "-storetype", "PKCS12", "-storepass", "changeit",
                "-keypass", "changeit", "-alias", "p", "-keyalg", "EC", "-groupname", "secp256r1", "-dname", "CN=test-phone", "-validity", "30");
        KeyStore ks = KeyStore.getInstance("PKCS12");
        try (InputStream in = Files.newInputStream(p12.toPath())) { ks.load(in, "changeit".toCharArray()); }
        final byte[] own = MessageDigest.getInstance("SHA-256").digest(((X509Certificate) ks.getCertificate("p")).getPublicKey().getEncoded());
        KeyManagerFactory kmf = KeyManagerFactory.getInstance(KeyManagerFactory.getDefaultAlgorithm());
        kmf.init(ks, "changeit".toCharArray());
        final SSLContext ctx = SSLContext.getInstance("TLS");
        ctx.init(kmf.getKeyManagers(), new TrustManager[] {new X509TrustManager() {
            public void checkClientTrusted(X509Certificate[] c, String t) { }
            public void checkServerTrusted(X509Certificate[] c, String t) { }
            public X509Certificate[] getAcceptedIssuers() { return new X509Certificate[0]; }
        }}, null);

        EndpointConnector.Opener opener = ep -> {
            if (ep.kind == org.qindaqt.link.engine.Endpoint.Kind.USB) throw new IOException("no USB in this test");
            Socket raw = new Socket();
            raw.connect(new InetSocketAddress(ep.host, ep.port), 3000);
            final SSLSocket tls = (SSLSocket) ctx.getSocketFactory().createSocket(raw, ep.host, ep.port, true);
            tls.setEnabledProtocols(new String[] {"TLSv1.3"});
            tls.startHandshake();
            try {
                final byte[] peer = MessageDigest.getInstance("SHA-256").digest(tls.getSession().getPeerCertificates()[0].getPublicKey().getEncoded());
                return new Link() {
                    public InputStream in() throws IOException { return tls.getInputStream(); }
                    public OutputStream out() throws IOException { return tls.getOutputStream(); }
                    public byte[] peerSpki() { return peer; }
                    public byte[] ownSpki() { return own; }
                    public void close() { try { tls.close(); } catch (IOException e) { /* ignore */ } }
                };
            } catch (java.security.GeneralSecurityException e) {
                throw new IOException(e);
            }
        };
        StatusSource st = () -> SessionTest.m("battery", SessionTest.m("level", 55L, "charging", false, "plugged", "none"),
                "network", SessionTest.m("type", "wifi", "signal", Json.NULL));
        EngineState state = new EngineState();
        PinStore pins = new PinStore(new File(tmp, "phone-pin"));
        // 127.0.0.2 is refused as a manual address (loopback), so the stand-in network is advertised by a fake NSD.
        final int port2 = lanPort;
        org.qindaqt.link.engine.DesktopDiscovery nsd = new org.qindaqt.link.engine.DesktopDiscovery() {
            public void start(org.qindaqt.link.engine.DiscoveryTable sink) {
                java.util.Map<String, String> txt = new java.util.HashMap<>();
                txt.put("proto", "1"); txt.put("id", "1111111111111111"); txt.put("name", "qinda-test");
                sink.found("qinda-test", org.qindaqt.link.engine.DiscoveredDesktop.fromTxt("qinda-test", "127.0.0.2", port2, txt));
            }
            public void stop() { }
        };
        EndpointConnector connector = new EndpointConnector(opener, pins, new LanMemory(new File(tmp, "phone-lan")), nsd);
        connector.selectDesktop("1111111111111111"); // unpaired phones connect only to a chosen desktop
        LinkEngine engine = new LinkEngine(connector, pins, new FeatureRegistry().add(new StatusFeature(st, state)),
                new DeviceInfo("xlan-phone", "xlan", "JVM", "0"), state, new Backoff(new long[] {200, 400}, new java.util.Random(3)), 60000);
        engine.start();
        waitPhase(state, EngineState.Phase.CONNECTED, 10000);
        expect(state.transport().equals("lan"), "engine connected over lan to the 127.0.0.2 listener");

        // The window is closed: pairing is refused with the explanatory message.
        engine.command(Session.Command.PAIR);
        waitMessage(state, "not accepting Wi-Fi pairing", 10000);
        expect(true, "pairing refused while the window is closed");

        CrossImplTest.gdbus("OpenPairingWindow");
        waitPhase(state, EngineState.Phase.CONNECTED, 10000); // the engine reconnected after the refusal
        engine.command(Session.Command.PAIR);
        waitPhase(state, EngineState.Phase.CONFIRM, 10000);
        String id = Pairing.hex(own);
        Thread.sleep(300);
        String phoneCode = ((java.util.Map<?, ?>) Json.parse(state.toJson())).get("code").toString().replace(" ", "");
        String mon = new String(Files.readAllBytes(new File(tmp, "monitor.log").toPath()), StandardCharsets.UTF_8);
        Matcher cm = Pattern.compile("PairingRequested \\('[0-9a-f]+', '[^']*', '[^']*', '(\\d{6})'\\)").matcher(mon);
        String desktopCode = cm.find() ? cm.group(1) : "";
        expect(phoneCode.length() == 6 && phoneCode.equals(desktopCode), "pairing codes equal over the LAN path");
        CrossImplTest.gdbus("ConfirmPairing", id, "true");
        engine.command(Session.Command.ACCEPT);
        waitPhase(state, EngineState.Phase.READY, 10000);
        boolean seen = false;
        for (dl = System.currentTimeMillis() + 10000; !seen && System.currentTimeMillis() < dl; Thread.sleep(300)) {
            String devices = CrossImplTest.gdbus("Devices");
            seen = devices.contains(id) && devices.contains("\"transport\":\"lan\"") && devices.contains("\"level\":55");
        }
        expect(seen, "Devices() lists the phone ready with transport lan");

        CrossImplTest.gdbus("SetLanEnabled", "false");
        for (dl = System.currentTimeMillis() + 10000; state.phase() == EngineState.Phase.READY && System.currentTimeMillis() < dl; Thread.sleep(100)) { }
        expect(state.phase() != EngineState.Phase.READY, "disabling LAN ends the LAN session");
        engine.stop();
        monitor.destroy();
        CrossImplTest.cleanup();
        System.out.println("cross-implementation LAN test OK");
        System.exit(0);
    }

    static void waitPhase(EngineState s, EngineState.Phase p, long ms) throws Exception {
        for (long dl = System.currentTimeMillis() + ms; System.currentTimeMillis() < dl; Thread.sleep(50))
            if (s.phase() == p) return;
        expect(false, "phase " + p + " not reached, at " + s.phase() + " " + s.toJson());
    }

    static void waitMessage(EngineState s, String part, long ms) throws Exception {
        for (long dl = System.currentTimeMillis() + ms; System.currentTimeMillis() < dl; Thread.sleep(50))
            if (s.toJson().contains(part)) return;
        expect(false, "message containing '" + part + "' not seen: " + s.toJson());
    }
}
