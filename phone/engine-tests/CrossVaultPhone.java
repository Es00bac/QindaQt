import java.io.BufferedReader;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.KeyStore;
import java.security.MessageDigest;
import java.security.cert.X509Certificate;
import java.util.concurrent.TimeUnit;
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
import org.qindaqt.link.proto.Json;
import org.qindaqt.link.vault.*;

/**
 * A real JVM phone (real Session, JSSE TLS 1.3, VaultTypeFeature and VaultTypeFlow) for the C++ test
 * tst_link_vault_cross. The C++ side owns the desktop service, the fake portal and the fake focus
 * publisher; this process talks to it over TLS on the loopback port in args[0] and to the harness over
 * stdin/stdout lines. Throwaway data only. Output never contains a secret, only the desktop's window facts.
 *
 * Harness lines: out "CODE <six digits>" then in "ACCEPT"; out "READY"; then repeated
 * in "TYPE <username|password|autotype>" -> out "CONFIRM <app>|<title>" or "RESULT failed <message>";
 * in "CONFIRM" or "CANCEL" -> out "RESULT <phase> <message>".
 */
public final class CrossVaultPhone {
    static void say(String s) { System.out.println(s); System.out.flush(); }

    static boolean until(java.util.function.BooleanSupplier c, long ms) throws Exception {
        for (long end = System.currentTimeMillis() + ms; System.currentTimeMillis() < end; Thread.sleep(20)) if (c.getAsBoolean()) return true;
        return false;
    }

    public static void main(String[] a) throws Exception {
        int port = Integer.parseInt(a[0]);
        File tmp = Files.createTempDirectory("qlink-xv").toFile();
        File p12 = new File(tmp, "phone.p12");
        Process kt = new ProcessBuilder("keytool", "-genkeypair", "-keystore", p12.getPath(), "-storetype", "PKCS12", "-storepass", "changeit",
                "-keypass", "changeit", "-alias", "p", "-keyalg", "EC", "-groupname", "secp256r1", "-dname", "CN=xv-phone", "-validity", "30")
                .redirectErrorStream(true).start();
        kt.getInputStream().readAllBytes();
        if (!kt.waitFor(30, TimeUnit.SECONDS) || kt.exitValue() != 0) throw new IOException("keytool failed");
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

        VaultSession vault = VaultTests.fresh();
        VaultRecord rec = VaultTests.rec("x", "user-X", "pw-X");
        String id = vault.put(rec);
        VaultTypeFeature feature = new VaultTypeFeature();
        VaultTypeFlow flow = new VaultTypeFlow(feature, () -> vault, System::currentTimeMillis);
        EngineState state = new EngineState();
        Session s = new Session(link, new PinStore(new File(tmp, "pin")), new FeatureRegistry().add(feature),
                new DeviceInfo("xv-phone", "xv", "JVM", "0"), state);
        Thread t = new Thread(s::run);
        t.start();
        if (!until(() -> state.phase() == EngineState.Phase.CONNECTED, 10000)) { say("ERROR not connected"); System.exit(2); }
        s.command(Session.Command.PAIR);
        if (!until(() -> state.phase() == EngineState.Phase.CONFIRM, 10000)) { say("ERROR no confirm"); System.exit(2); }
        Thread.sleep(300);
        say("CODE " + String.valueOf(((java.util.Map<?, ?>) Json.parse(state.toJson())).get("code")).replace(" ", "") + " " + Pairing_hex(own));
        BufferedReader in = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
        if (!"ACCEPT".equals(in.readLine())) System.exit(2);
        s.command(Session.Command.ACCEPT);
        if (!until(() -> state.phase() == EngineState.Phase.READY, 10000)) { say("ERROR not ready"); System.exit(2); }
        Thread.sleep(300);   // the desktop reaches READY a moment later; the capability list is exchanged by then
        say("READY");
        for (String line; (line = in.readLine()) != null; ) {
            if (line.startsWith("TYPE ")) {
                String f = line.substring(5);
                flow.begin(id, f.equals("username") ? VaultTypingSource.Field.USERNAME : f.equals("password") ? VaultTypingSource.Field.PASSWORD : VaultTypingSource.Field.AUTOTYPE);
                until(() -> flow.phase() != VaultTypeFlow.Phase.REQUESTING, 8000);
                if (flow.phase() == VaultTypeFlow.Phase.CONFIRM) say("CONFIRM " + flow.app() + "|" + flow.title());
                else say("RESULT " + flow.phase().name().toLowerCase() + " " + flow.message());
            } else if (line.equals("CONFIRM") || line.equals("CANCEL")) {
                if (line.equals("CANCEL")) flow.cancel(); else flow.confirm();
                until(() -> flow.phase() != VaultTypeFlow.Phase.SENDING, 20000);
                say("RESULT " + flow.phase().name().toLowerCase() + " " + flow.message());
            } else if (line.equals("QUIT")) {
                break;
            }
        }
        s.command(Session.Command.STOP);
        t.join(3000);
        System.exit(0);
    }

    static String Pairing_hex(byte[] b) { return org.qindaqt.link.proto.Pairing.hex(b); }
}
