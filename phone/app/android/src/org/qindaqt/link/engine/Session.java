package org.qindaqt.link.engine;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ArrayBlockingQueue;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import org.qindaqt.link.proto.Envelope;
import org.qindaqt.link.proto.FrameCodec;
import org.qindaqt.link.proto.Pairing;

/**
 * One phone-side connection: hello, pairing, keepalive, status. Android-free.
 *
 * AGENT-GUARD: all writes happen on the thread that calls {@link #run()};
 * UI actions arrive through {@link #command} and are applied on that thread.
 * Frame bodies are never logged (only types), see the protocol page's
 * vault redaction rule; this class does not know vault yet but keeps the habit.
 * Pairing follows docs/wiki/reference/qindalink-protocol-v1.md#pairing.
 */
public final class Session {
    /** UI actions. */
    public enum Command { PAIR, ACCEPT, REJECT, PAIR_AGAIN, STOP }

    private enum Phase { HELLO, UNPAIRED, REQUESTED, NONCE_SENT, CONFIRM, READY }

    private static final long HELLO_TIMEOUT_MS = 10000, PING_AFTER_MS = 30000, DEAD_AFTER_MS = 90000,
            PAIR_TIMEOUT_MS = 120000, WRITE_STALL_MS = 10000;
    private static final int EVENT_CAPACITY = 128, WRITE_CAPACITY = 64;
    private static final Object WAKE = new Object();

    private final Link link;
    private final PinStore pins;
    private final FeatureRegistry features;
    private final DeviceInfo info;
    private final EngineState state;
    // AGENT-GUARD: both queues are bounded. The reader blocks when events is full (TCP backpressure,
    // the 90 s silence timer still runs on the main loop); commands and wakeups are dropped when full.
    private final BlockingQueue<Object> events = new ArrayBlockingQueue<>(EVENT_CAPACITY);
    private final BlockingQueue<byte[]> outbox = new ArrayBlockingQueue<>(WRITE_CAPACITY);
    private static final byte[] END = new byte[0];
    private volatile long writeStartedMs;
    private volatile boolean done;
    private Thread writer;
    private final FrameCodec decoder = new FrameCodec();
    private final SecureRandom rng = new SecureRandom();

    private Phase phase = Phase.HELLO;
    private String desktopName = "";
    private List<?> desktopCaps = new ArrayList<>();
    private boolean desktopTrusted, pinMatches, stale;
    private final Map<Long, FeatureContext.ReplyHandler> replies = new java.util.HashMap<>();
    private long nextId = 1, requestId, lastSentMs, lastRecvMs, pairStartMs;
    private byte[] np, nd, commit;
    private String code = "";
    private boolean localAccepted, remoteAccepted, readyHandlers;
    private final String peerId;

    /** Convenience for tests and simple hosts: the registry holds only the status feature. */
    public Session(Link link, PinStore pins, StatusSource status, DeviceInfo info, EngineState state) {
        this(link, pins, new FeatureRegistry().add(new StatusFeature(status, state)), info, state);
    }

    public Session(Link link, PinStore pins, FeatureRegistry features, DeviceInfo info, EngineState state) {
        this.link = link;
        this.pins = pins;
        this.features = features;
        this.info = info;
        this.state = state;
        this.peerId = Pairing.hex(link.peerSpki());
    }

    /** Thread-safe: queue a UI action. */
    public void command(Command c) { events.offer(c); }

    /** Runs until the connection ends. Always closes the link. */
    public void run() {
        Thread reader = new Thread(this::readLoop, "qindalink-reader");
        reader.setDaemon(true);
        writer = new Thread(this::writeLoop, "qindalink-writer");
        writer.setDaemon(true);
        try {
            String pinned = pins.deviceId();
            pinMatches = peerId.equals(pinned);
            if (pinned != null && !pinMatches) {
                // AGENT-NOTE: a different key than the pinned desktop. Never fall back to pairing here.
                state.set(EngineState.Phase.ERROR, "", "", "A different desktop answered; forget the paired desktop to pair a new one.");
                return;
            }
            writer.start();
            reader.start();
            sendHello();
            loop();
        } catch (IOException e) {
            if (state.phase() != EngineState.Phase.ERROR) state.set(EngineState.Phase.DISCONNECTED, desktopName, "", "Connection lost");
        } finally {
            if (readyHandlers) for (FeatureHandler h : features.handlers()) h.onClosed();
            done = true;
            outbox.offer(END); // lets a queued bye flush; the watchdog bounds how long this can take
            try { writer.join(1500); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
            link.close();
        }
    }

    /** Sole owner of link.out(). A stalled write is detected by onTick's watchdog, which closes the link. */
    private void writeLoop() {
        try {
            OutputStream out = link.out();
            while (true) {
                byte[] f = outbox.take();
                if (f == END) return;
                writeStartedMs = now();
                out.write(f);
                out.flush();
                writeStartedMs = 0;
            }
        } catch (IOException | InterruptedException e) {
            if (!done) putEvent(e instanceof IOException ? e : new IOException("writer interrupted"));
        }
    }

    /** Reader/writer side hand-off with backpressure; gives up once the session is over. */
    private void putEvent(Object o) {
        try {
            while (!done) if (events.offer(o, 200, TimeUnit.MILLISECONDS)) return;
        } catch (InterruptedException ignored) { /* thread ends */ }
    }

    private void readLoop() {
        byte[] buf = new byte[8192];
        try {
            InputStream in = link.in();
            while (true) {
                int n = in.read(buf);
                if (n < 0) { putEvent(new IOException("eof")); return; }
                decoder.feed(buf, 0, n);
                FrameCodec.Result r;
                while ((r = decoder.next()).status == FrameCodec.Status.FRAME) putEvent(r.envelope);
                if (r.status != FrameCodec.Status.NEEDS_MORE) { putEvent(new IOException("protocol error " + r.status)); return; }
            }
        } catch (IOException e) {
            putEvent(e);
        }
    }

    private void loop() throws IOException {
        long start = now();
        lastRecvMs = start;
        while (true) {
            Object ev;
            try {
                ev = events.poll(500, TimeUnit.MILLISECONDS);
            } catch (InterruptedException e) {
                return;
            }
            long t = now();
            if (ev instanceof IOException) throw (IOException) ev;
            if (ev instanceof Envelope) { lastRecvMs = t; if (!onFrame((Envelope) ev)) return; }
            if (ev instanceof Command && !onCommand((Command) ev)) return;
            if (!onTick(t, start)) return;
        }
    }

    private boolean onTick(long t, long start) throws IOException {
        long ws = writeStartedMs;
        if (ws != 0 && t - ws > WRITE_STALL_MS) { link.close(); throw new IOException("write stalled"); }
        if (phase == Phase.HELLO && t - start > HELLO_TIMEOUT_MS) throw new IOException("hello timeout");
        if (t - lastRecvMs > DEAD_AFTER_MS) throw new IOException("peer silent");
        if (t - lastSentMs > PING_AFTER_MS) send(new Envelope("ping", nextId++, 0, null));
        if ((phase == Phase.REQUESTED || phase == Phase.NONCE_SENT || phase == Phase.CONFIRM) && t - pairStartMs > PAIR_TIMEOUT_MS) {
            bye("pair-timeout");
            state.set(EngineState.Phase.ERROR, desktopName, "", "Pairing timed out");
            return false;
        }
        if (phase == Phase.READY) for (FeatureHandler h : features.handlers()) h.onTick(t);
        return true;
    }

    private boolean onFrame(Envelope e) throws IOException {
        // AGENT-CONTRACT: a reply to a feature's request() goes to that feature whatever its type (ok, error,
        // or a typed frame such as desktop.focus), before the core frames are interpreted.
        if (e.re != 0 && phase == Phase.READY) {
            FeatureContext.ReplyHandler rh = replies.remove(e.re);
            if (rh != null) { rh.onReply(e); return true; }
        }
        switch (e.t) {
            case "hello": return onHello(e);
            case "ping": send(new Envelope("pong", 0, e.id, null)); return true;
            case "pong": case "ok": return true;
            case "bye": state.set(EngineState.Phase.DISCONNECTED, desktopName, "", "Desktop closed: " + str(e.b.get("reason"))); return false;
            case "error": return onError(e);
            default: break;
        }
        if (phase == Phase.HELLO) throw new IOException("frame before hello");
        if (e.t.startsWith("pair.")) return onPair(e);
        if (phase != Phase.READY) throw new IOException("frame not allowed unpaired: " + e.t);
        FeatureHandler h = features.find(e.t);
        if (h != null && h.onFrame(e)) return true;
        if (e.id != 0) {
            // AGENT-NOTE: `ring` without a registered handler is answered honestly instead of ringing the user's phone.
            Map<String, Object> b = new LinkedHashMap<>();
            boolean ring = e.t.equals("ring");
            b.put("code", ring ? "unsupported" : "unknown-type");
            b.put("message", ring ? "ring is not implemented in this build" : "no handler");
            send(new Envelope("error", 0, e.id, b));
        }
        return true;
    }

    private boolean onHello(Envelope e) throws IOException {
        if (phase != Phase.HELLO) throw new IOException("duplicate hello");
        Object proto = e.b.get("proto");
        if (!(proto instanceof Long) || (Long) proto != 1L || !"desktop".equals(e.b.get("role"))) {
            bye("protocol-version");
            state.set(EngineState.Phase.ERROR, "", "", "Desktop speaks another protocol version");
            return false;
        }
        desktopName = str(e.b.get("name"));
        desktopCaps = e.b.get("caps") instanceof List ? (List<?>) e.b.get("caps") : new ArrayList<>();
        desktopTrusted = Boolean.TRUE.equals(e.b.get("trusted"));
        state.setPaired(pinMatches, pins.name());
        if (pinMatches && desktopTrusted) { becomeReady(); return true; }
        phase = Phase.UNPAIRED;
        stale = pinMatches; // we pinned it but it forgot us
        state.set(stale ? EngineState.Phase.STALE : EngineState.Phase.CONNECTED, desktopName, "",
                stale ? "The desktop forgot this phone" : "Desktop found");
        return true;
    }

    private boolean onError(Envelope e) {
        if (phase == Phase.REQUESTED && e.re == requestId) {
            // AGENT-NOTE: over LAN the desktop answers "denied" until the user opens its pairing window (ADR-0434 item 5).
            String code = str(e.b.get("code"));
            state.set(EngineState.Phase.ERROR, desktopName, "", "denied".equals(code)
                    ? "The desktop is not accepting Wi-Fi pairing. Open the pairing window on the desktop and try again."
                    : "Desktop refused pairing: " + code);
            return false;
        }
        return true;
    }

    private boolean onCommand(Command c) throws IOException {
        switch (c) {
            case STOP: bye("shutdown"); return false;
            case PAIR_AGAIN:
                if (phase != Phase.UNPAIRED || !stale) return true;
                pins.clear();
                pinMatches = false;
                state.setPaired(false, null);
                return startPairing();
            case PAIR:
                if (phase != Phase.UNPAIRED || stale) return true;
                return startPairing();
            case ACCEPT:
                if (phase != Phase.CONFIRM) return true;
                localAccepted = true;
                send(confirm(true));
                return maybeFinish();
            case REJECT:
                if (phase != Phase.CONFIRM) return true;
                send(confirm(false));
                bye("pair-rejected");
                state.set(EngineState.Phase.ERROR, desktopName, "", "Pairing rejected");
                return false;
            default: return true;
        }
    }

    private boolean startPairing() throws IOException {
        phase = Phase.REQUESTED;
        requestId = nextId++;
        pairStartMs = now();
        state.set(EngineState.Phase.PAIRING, desktopName, "", "Starting pairing");
        send(new Envelope("pair.request", requestId, 0, null));
        return true;
    }

    private boolean onPair(Envelope e) throws IOException {
        switch (e.t) {
            case "pair.commit":
                if (phase != Phase.REQUESTED || e.re != requestId) throw new IOException("unexpected commit");
                commit = Pairing.unhex(str(e.b.get("c")), 32);
                if (commit == null) return rejectMalformed("commit");
                np = new byte[32];
                rng.nextBytes(np);
                phase = Phase.NONCE_SENT;
                send(Envelope.of("pair.nonce", map("np", Pairing.hex(np))));
                return true;
            case "pair.reveal":
                if (phase != Phase.NONCE_SENT) throw new IOException("unexpected reveal");
                nd = Pairing.unhex(str(e.b.get("nd")), 32);
                if (nd == null) return rejectMalformed("reveal");
                if (!Pairing.verifyCommitment(commit, nd, link.peerSpki(), link.ownSpki())) {
                    bye("pair-rejected");
                    state.set(EngineState.Phase.ERROR, desktopName, "", "Pairing check failed; possible interception");
                    return false;
                }
                code = Pairing.code(link.peerSpki(), link.ownSpki(), nd, np);
                phase = Phase.CONFIRM;
                state.set(EngineState.Phase.CONFIRM, desktopName, Pairing.display(code), "Compare the code with the desktop");
                return true;
            case "pair.confirm":
                if (phase != Phase.CONFIRM) throw new IOException("unexpected confirm");
                if (!(e.b.get("accept") instanceof Boolean)) throw new IOException("bad confirm");
                if (!(Boolean) e.b.get("accept")) {
                    bye("pair-rejected");
                    state.set(EngineState.Phase.ERROR, desktopName, "", "The desktop rejected pairing");
                    return false;
                }
                remoteAccepted = true;
                return maybeFinish();
            default:
                if (e.id != 0) send(new Envelope("error", 0, e.id, map("code", "unknown-type", "message", "no handler")));
                return true;
        }
    }

    /** Protocol: a c/np/nd that is not exactly 64 lowercase hex ends pairing with pair-rejected. */
    private boolean rejectMalformed(String what) {
        bye("pair-rejected");
        state.set(EngineState.Phase.ERROR, desktopName, "", "Desktop sent a malformed pairing " + what);
        return false;
    }

    private boolean maybeFinish() throws IOException {
        if (!(localAccepted && remoteAccepted)) return true;
        try {
            pins.set(peerId, desktopName.isEmpty() ? "desktop" : desktopName);
        } catch (IOException e) {
            // Protocol: never enter READY with a pin we could not persist, or the stores disagree after restart.
            pins.clear();
            // AGENT-NOTE: publish ERROR before bye so anything that observes the bye
            // (the peer, tests) already sees the final local state; the reverse order raced.
            state.set(EngineState.Phase.ERROR, desktopName, "", "Could not save the pairing");
            bye("error");
            return false;
        }
        pinMatches = true;
        state.setPaired(true, desktopName);
        becomeReady();
        return true;
    }

    private void becomeReady() throws IOException {
        phase = Phase.READY;
        state.set(EngineState.Phase.READY, desktopName, "", "Connected");
        readyHandlers = true;
        for (FeatureHandler h : features.handlers()) h.onReady(new Ctx(h));
    }

    /** Per-handler view of this session; enforces the namespace and capability rules centrally. */
    private final class Ctx implements FeatureContext {
        private final FeatureHandler h;
        Ctx(FeatureHandler h) { this.h = h; }

        @Override public boolean send(Envelope e) throws IOException {
            int dot = e.t.indexOf('.');
            String ns = dot < 0 ? e.t : e.t.substring(0, dot);
            if (phase != Phase.READY || !h.namespaces().contains(ns) || !desktopCaps.contains(h.capability())) return false;
            Session.this.send(e);
            return true;
        }
        @Override public boolean request(String type, Map<String, Object> body, ReplyHandler handler) throws IOException {
            int dot = type.indexOf('.');
            String ns = dot < 0 ? type : type.substring(0, dot);
            if (phase != Phase.READY || !h.namespaces().contains(ns) || !desktopCaps.contains(h.capability())) return false;
            long id = nextId++;
            replies.put(id, handler);
            try { Session.this.send(new Envelope(type, id, 0, body)); }
            catch (IOException ex) { replies.remove(id); throw ex; }
            return true;
        }
        @Override public long nextId() { return nextId++; }
        @Override public boolean peerHas(String capability) { return desktopCaps.contains(capability); }
        @Override public void replyOk(long id) throws IOException { Session.this.send(new Envelope("ok", 0, id, null)); }
        @Override public void replyError(long id, String code, String message) throws IOException {
            Session.this.send(new Envelope("error", 0, id, map("code", code, "message", message)));
        }
        @Override public void wake() { events.offer(WAKE); }
    }

    private void sendHello() throws IOException {
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("proto", 1L);
        b.put("role", "phone");
        b.put("name", info.name);
        b.put("model", info.model);
        b.put("os", info.os);
        b.put("app", info.app);
        List<Object> caps = new ArrayList<>();
        caps.addAll(features.caps()); // `ring` is advertised only once a handler for it is registered.
        b.put("caps", caps);
        b.put("trusted", pinMatches);
        send(Envelope.of("hello", b));
        state.set(EngineState.Phase.CONNECTING, "", "", "Waiting for the desktop");
    }

    private void bye(String reason) {
        try { send(Envelope.of("bye", map("reason", reason))); } catch (IOException ignored) { /* closing anyway */ }
    }

    private Envelope confirm(boolean accept) {
        Map<String, Object> b = new LinkedHashMap<>();
        b.put("accept", accept);
        return Envelope.of("pair.confirm", b);
    }

    private void send(Envelope e) throws IOException {
        if (!outbox.offer(FrameCodec.encode(e))) throw new IOException("write backlog");
        lastSentMs = now();
    }

    private static Map<String, Object> map(String... kv) {
        Map<String, Object> m = new LinkedHashMap<>();
        for (int i = 0; i < kv.length; i += 2) m.put(kv[i], kv[i + 1]);
        return m;
    }

    private static String str(Object o) { return o instanceof String ? (String) o : ""; }

    private static long now() { return System.nanoTime() / 1000000L; }
}
