import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PipedInputStream;
import java.io.PipedOutputStream;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import org.qindaqt.link.engine.Backoff;
import org.qindaqt.link.engine.DesktopDiscovery;
import org.qindaqt.link.engine.DeviceInfo;
import org.qindaqt.link.engine.DiscoveredDesktop;
import org.qindaqt.link.engine.DiscoveryTable;
import org.qindaqt.link.engine.Endpoint;
import org.qindaqt.link.engine.EndpointConnector;
import org.qindaqt.link.engine.EndpointPlanner;
import org.qindaqt.link.engine.EngineState;
import org.qindaqt.link.engine.FeatureRegistry;
import org.qindaqt.link.engine.LanMemory;
import org.qindaqt.link.engine.Link;
import org.qindaqt.link.engine.LinkEngine;
import org.qindaqt.link.engine.ManualAddress;
import org.qindaqt.link.engine.PinStore;

/** LAN and Tailscale connection logic with a fake NSD and fake TLS opener (no Android, no sockets to a real desktop). */
public final class LanTest {
    static void expect(boolean ok, String what) {
        if (!ok) { System.out.println("FAIL " + what); System.exit(1); }
    }

    static final String PIN = "0".repeat(64);
    static final String OTHER = "fedcba9876543210";

    static DiscoveredDesktop desk(String id, String name, String host) {
        Map<String, String> txt = new HashMap<>();
        txt.put("proto", "1");
        txt.put("id", id);
        txt.put("name", name);
        return DiscoveredDesktop.fromTxt(name, host, 47160, txt);
    }

    /** Fake NSD: records start/stop and lets a test push desktops into the table. */
    static final class FakeNsd implements DesktopDiscovery {
        int starts, stops;
        DiscoveryTable sink;
        @Override public void start(DiscoveryTable s) { starts++; sink = s; }
        @Override public void stop() { stops++; }
    }

    /** Fake link whose reader blocks until closed; the Session sits in its hello wait. */
    static byte[] zeros32 = new byte[32];
    static final class IdleLink implements Link {
        byte[] spki = zeros32;
        final PipedOutputStream feed = new PipedOutputStream();
        final PipedInputStream in;
        volatile boolean closed;
        IdleLink() throws IOException { in = new PipedInputStream(feed); }
        public InputStream in() { return in; }
        public OutputStream out() { return new OutputStream() { public void write(int b) { } }; }
        public byte[] peerSpki() { return spki; }
        public byte[] ownSpki() { return new byte[32]; }
        public void close() { closed = true; try { feed.close(); } catch (IOException ignored) { } }
    }

    static final class FakeOpener implements EndpointConnector.Opener {
        final List<String> attempts = new ArrayList<>();
        final java.util.Set<String> reachable = new java.util.HashSet<>(); // host:port that answer
        final Map<String, byte[]> keys = new HashMap<>();                   // host:port -> key it presents (default zeros)
        final List<IdleLink> opened = new ArrayList<>();
        public Link open(Endpoint e) throws IOException {
            attempts.add(e.kind + ":" + e.hostPort());
            if (!reachable.contains(e.hostPort())) throw new IOException("unreachable");
            IdleLink l = new IdleLink();
            if (keys.containsKey(e.hostPort())) l.spki = keys.get(e.hostPort());
            opened.add(l);
            return l;
        }
    }

    static void manualAddress() {
        Endpoint e = ManualAddress.parse("100.64.0.10:47160");
        expect(e.host.equals("100.64.0.10") && e.port == 47160 && e.kind == Endpoint.Kind.LAN_MANUAL, "ipv4 host:port");
        expect(ManualAddress.parse("desktop.tail0000.ts.net").port == 47160, "magicdns default port");
        expect(ManualAddress.parse("desktop.tail0000.ts.net:5000").port == 5000, "magicdns custom port");
        expect(ManualAddress.parse("  10.0.0.182  ").host.equals("10.0.0.182"), "trimmed");
        Endpoint v6 = ManualAddress.parse("[fd7a:115c:a1e0::1]:47161");
        expect(v6.host.equals("fd7a:115c:a1e0::1") && v6.port == 47161, "bracketed ipv6");
        expect(ManualAddress.parse("fd7a:115c:a1e0::1").host.equals("fd7a:115c:a1e0::1"), "bare ipv6");
        expect(ManualAddress.parse(v6.hostPort()).equals(v6), "hostPort round trip");
        for (String bad : new String[] {"", "   ", null, "host:", "host:0", "host:70000", "host:abc", "300.1.1.1", "1.2.3",
                "-bad.example", "bad_name", "http://example.com", "a b", "[::1", "[fd7a::1]x", "example..com",
                "127.0.0.1", "localhost:47160", "::1", "[::1]:47160", "0.0.0.0", "::ffff:127.0.0.1", "[::ffff:127.0.0.1]:47160", "[::ffff:7f00:1]:47160", "::FFFF:127.0.0.9"}) {
            try { ManualAddress.parse(bad); expect(false, "accepted bad address: " + bad); }
            catch (IllegalArgumentException expected) { expect(expected.getMessage().length() > 5, "message for " + bad); }
        }
    }

    static void txtFiltering() {
        expect(desk("0000000000000000", "qinda", "10.0.0.182") != null, "valid txt");
        Map<String, String> txt = new HashMap<>();
        txt.put("proto", "2"); txt.put("id", "0000000000000000");
        expect(DiscoveredDesktop.fromTxt("n", "h", 47160, txt) == null, "wrong proto rejected");
        txt.put("proto", "1"); txt.put("id", "0123456789ABCDEF");
        expect(DiscoveredDesktop.fromTxt("n", "h", 47160, txt) == null, "uppercase id rejected");
        txt.put("id", "0123");
        expect(DiscoveredDesktop.fromTxt("n", "h", 47160, txt) == null, "short id rejected");
        txt.remove("id");
        expect(DiscoveredDesktop.fromTxt("n", "h", 47160, txt) == null, "missing id rejected");
        txt.put("id", "0000000000000000");
        expect(DiscoveredDesktop.fromTxt("n", "", 47160, txt) == null, "no host rejected");
        expect(DiscoveredDesktop.fromTxt("n", "h", 0, txt) == null, "bad port rejected");
        expect(DiscoveredDesktop.fromTxt("inst", "h", 47160, txt).name.equals("inst"), "name falls back to the instance");
        txt.put("name", "a\u0007b");
        expect(DiscoveredDesktop.fromTxt("n", "h", 47160, txt).name.equals("ab"), "control characters stripped");
    }

    static void planning() {
        List<DiscoveredDesktop> seen = new ArrayList<>();
        seen.add(desk(OTHER, "intruder", "10.0.0.50"));
        seen.add(desk("0000000000000000", "qinda", "10.0.0.182"));
        Endpoint manual = ManualAddress.parse("100.64.0.10");
        Endpoint last = new Endpoint("10.0.0.99", 47160, Endpoint.Kind.LAN_LAST);
        List<Endpoint> p = EndpointPlanner.plan(PIN, null, last, seen, manual);
        expect(p.size() == 4, "paired plan size " + p.size());
        expect(p.get(0).kind == Endpoint.Kind.USB, "usb first");
        expect(p.get(1).kind == Endpoint.Kind.LAN_LAST, "last lan second");
        expect(p.get(2).kind == Endpoint.Kind.LAN_DISCOVERED && p.get(2).host.equals("10.0.0.182"), "matching desktop third");
        expect(p.get(3).kind == Endpoint.Kind.LAN_MANUAL, "manual last");
        for (Endpoint e : p) expect(!e.host.equals("10.0.0.50"), "id mismatch never planned when paired");

        // Unpaired without a selection: no discovered desktop is attempted.
        p = EndpointPlanner.plan(null, null, null, seen, null);
        expect(p.size() == 1 && p.get(0).kind == Endpoint.Kind.USB, "unpaired, nothing selected: usb only");
        p = EndpointPlanner.plan(null, OTHER, null, seen, null);
        expect(p.size() == 2 && p.get(1).host.equals("10.0.0.50"), "unpaired, selected desktop planned");

        // The same address under two sources appears once, at its first position.
        p = EndpointPlanner.plan(PIN, null, new Endpoint("10.0.0.182", 47160, Endpoint.Kind.LAN_LAST), seen,
                new Endpoint("10.0.0.182", 47160, Endpoint.Kind.LAN_MANUAL));
        expect(p.size() == 2, "deduplicated");
    }

    static void backoff() {
        long[] ladder = {1000, 2000, 4000, 8000, 15000};
        Backoff b = new Backoff(ladder, new Random(42));
        for (int f = 0; f <= 9; f++) {
            long base = ladder[Math.min(Math.max(f - 1, 0), 4)];
            for (int i = 0; i < 200; i++) {
                long d = b.delayMs(f);
                expect(d >= base / 2 && d <= base, "delay " + d + " within [" + base / 2 + "," + base + "] for failures=" + f);
            }
        }
        boolean varied = false;
        long first = b.delayMs(3);
        for (int i = 0; i < 20; i++) varied |= b.delayMs(3) != first;
        expect(varied, "jitter varies");
        expect(new Backoff(ladder, new Random(7)).delayMs(5) == new Backoff(ladder, new Random(7)).delayMs(5), "deterministic per seed");
    }

    static void memory() throws Exception {
        File dir = Files.createTempDirectory("qlink-lan").toFile();
        File f = new File(dir, "lan");
        LanMemory m = new LanMemory(f);
        expect(m.manual() == null && m.lastLan() == null && m.manualText().isEmpty(), "empty at first");
        m.setManual("100.64.0.10");
        m.setLastLan(new Endpoint("10.0.0.182", 47160, Endpoint.Kind.LAN_DISCOVERED));
        LanMemory again = new LanMemory(f);
        expect(again.manual().hostPort().equals("100.64.0.10:47160"), "manual persisted");
        expect(again.lastLan().hostPort().equals("10.0.0.182:47160"), "last lan persisted");
        try { again.setManual("not a host"); expect(false, "invalid manual accepted"); } catch (IllegalArgumentException expected) { }
        expect(again.manual().hostPort().equals("100.64.0.10:47160"), "invalid manual leaves the old one");
        again.setManual("");
        expect(again.manual() == null && again.lastLan() != null, "clearing manual keeps last lan");
        Files.write(f.toPath(), "garbage\nmanual=\n".getBytes());
        expect(new LanMemory(f).manual() == null, "garbage reads as none");
    }

    static EndpointConnector connector(FakeOpener op, FakeNsd nsd, File dir, String pinned) throws Exception {
        PinStore pins = new PinStore(new File(dir, "pin"));
        if (pinned != null) pins.set(pinned, "qinda");
        return new EndpointConnector(op, pins, new LanMemory(new File(dir, "lan")), nsd);
    }

    static void connectorOrderDiscoveryAndTransport() throws Exception {
        File dir = Files.createTempDirectory("qlink-conn").toFile();
        FakeOpener op = new FakeOpener();
        FakeNsd nsd = new FakeNsd();
        EndpointConnector c = connector(op, nsd, dir, PIN);
        c.setManualAddress("100.64.0.10");
        expect(c.transport().equals("none"), "no transport before connecting");

        // Nothing reachable: discovery starts once, every endpoint is tried in order, then IOException.
        nsd.starts = 0;
        try { c.connect(); expect(false, "connect should fail"); } catch (IOException expected) { }
        expect(nsd.starts == 1 && nsd.stops == 0, "discovery runs while disconnected");
        nsd.sink.found("qinda", desk("0000000000000000", "qinda", "10.0.0.182"));
        nsd.sink.found("other", desk(OTHER, "other", "10.0.0.50"));
        op.attempts.clear();
        try { c.connect(); expect(false, "connect should fail"); } catch (IOException expected) { }
        expect(nsd.starts == 1, "discovery not restarted while running");
        expect(op.attempts.toString().equals("[USB:127.0.0.1:47160, LAN_DISCOVERED:10.0.0.182:47160, LAN_MANUAL:100.64.0.10:47160]"),
                "attempt order " + op.attempts);

        // The manual address (Tailscale) answers: transport is lan, discovery stops, the address is remembered.
        op.reachable.add("100.64.0.10:47160");
        Link l = c.connect();
        expect(c.transport().equals("lan"), "transport lan");
        expect(nsd.stops == 1, "discovery stops once connected");
        l.close();
        c.disconnected();
        expect(c.transport().equals("none"), "transport none after disconnect");

        // Next time the remembered LAN address comes before everything but USB; discovery restarts.
        op.attempts.clear();
        op.reachable.clear();
        op.reachable.add("100.64.0.10:47160");
        c.connect().close();
        expect(op.attempts.toString().equals("[USB:127.0.0.1:47160, LAN_LAST:100.64.0.10:47160]"), "remembered address second " + op.attempts);
        c.disconnected();
        expect(nsd.starts == 2, "discovery restarts after a disconnect");

        // USB beats LAN whenever it answers.
        op.reachable.add("127.0.0.1:47160");
        c.connect().close();
        expect(c.transport().equals("usb"), "usb preferred");
        c.disconnected();
    }

    static void rogueEndpointDoesNotShadowThePinnedDesktop() throws Exception {
        File dir = Files.createTempDirectory("qlink-rogue").toFile();
        FakeOpener op = new FakeOpener();
        FakeNsd nsd = new FakeNsd();
        EndpointConnector c = connector(op, nsd, dir, PIN);
        c.setManualAddress("100.64.0.10");
        try { c.connect(); } catch (IOException expected) { }
        // A rogue host squats on the discovered address (right TXT id, wrong key); the real desktop is the manual one.
        nsd.sink.found("rogue", desk("0000000000000000", "qinda", "10.0.0.50"));
        byte[] wrong = new byte[32]; wrong[0] = 1;
        op.reachable.add("10.0.0.50:47160"); op.keys.put("10.0.0.50:47160", wrong);
        op.reachable.add("100.64.0.10:47160");
        op.attempts.clear(); op.opened.clear();
        Link l = c.connect();
        expect(op.attempts.toString().equals("[USB:127.0.0.1:47160, LAN_DISCOVERED:10.0.0.50:47160, LAN_MANUAL:100.64.0.10:47160]"), "rogue tried then skipped " + op.attempts);
        expect(c.transport().equals("lan"), "connected to the real endpoint");
        expect(op.opened.get(0).closed && !op.opened.get(1).closed, "rogue link closed, real link kept");
        expect(java.util.Arrays.equals(l.peerSpki(), zeros32), "the kept link has the pinned key");
        // A rogue USB answer does not count as an upgrade.
        op.reachable.add("127.0.0.1:47160"); op.keys.put("127.0.0.1:47160", wrong);
        expect(!c.upgradeAvailable(), "wrong-key usb is not an upgrade");
    }

    static void unpairedNeedsAChoice() throws Exception {
        File dir = Files.createTempDirectory("qlink-unp").toFile();
        FakeOpener op = new FakeOpener();
        FakeNsd nsd = new FakeNsd();
        EndpointConnector c = connector(op, nsd, dir, null);
        try { c.connect(); } catch (IOException expected) { }
        nsd.sink.found("a", desk("0000000000000000", "qinda", "10.0.0.182"));
        op.attempts.clear();
        try { c.connect(); } catch (IOException expected) { }
        expect(op.attempts.size() == 1, "unpaired phone does not wander to a discovered desktop: " + op.attempts);
        expect(c.uiJson().contains("qinda") && !c.uiJson().contains("10.0.0.182"), "ui lists the name, never the address");
        c.selectDesktop("0000000000000000");
        op.attempts.clear();
        op.reachable.add("10.0.0.182:47160");
        c.connect().close();
        expect(op.attempts.toString().equals("[USB:127.0.0.1:47160, LAN_DISCOVERED:10.0.0.182:47160]"), "chosen desktop is tried " + op.attempts);
        expect(c.setManualAddress("bad host").length() > 0 && c.uiJson().contains("manualError"), "manual error reported");
        expect(c.setManualAddress("100.64.0.10:47160").isEmpty(), "manual accepted");
    }

    static void engineReportsLanThenUpgradesToUsb() throws Exception {
        File dir = Files.createTempDirectory("qlink-eng").toFile();
        FakeOpener op = new FakeOpener();
        FakeNsd nsd = new FakeNsd();
        EndpointConnector c = connector(op, nsd, dir, PIN);
        c.setManualAddress("100.64.0.10");
        op.reachable.add("100.64.0.10:47160");
        EngineState state = new EngineState();
        LinkEngine engine = new LinkEngine(c, new PinStore(new File(dir, "pin")), new FeatureRegistry(),
                new DeviceInfo("m", "m", "Android", "0"), state, new Backoff(new long[] {20, 20}, new Random(1)), 100);
        engine.start();
        long t0 = System.currentTimeMillis();
        while (!state.transport().equals("lan") && System.currentTimeMillis() - t0 < 5000) Thread.sleep(10);
        expect(state.transport().equals("lan") && state.toJson().contains("\"transport\":\"lan\""), "engine reports lan");
        op.reachable.add("127.0.0.1:47160"); // the cable is plugged in
        t0 = System.currentTimeMillis();
        while (!state.transport().equals("usb") && System.currentTimeMillis() - t0 < 5000) Thread.sleep(10);
        expect(state.transport().equals("usb"), "engine moves to usb when it answers");
        engine.stop();
    }

    public static void main(String[] a) throws Exception {
        manualAddress();
        txtFiltering();
        planning();
        backoff();
        memory();
        connectorOrderDiscoveryAndTransport();
        rogueEndpointDoesNotShadowThePinnedDesktop();
        unpairedNeedsAChoice();
        engineReportsLanThenUpgradesToUsb();
        System.out.println("LanTest ok");
    }
}
