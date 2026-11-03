// SPDX-License-Identifier: LGPL-3.0-or-later
package org.qindaqt.link.vault;

import java.io.IOException;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicBoolean;
import org.qindaqt.link.engine.FeatureContext;
import org.qindaqt.link.engine.FeatureHandler;
import org.qindaqt.link.proto.Envelope;

/**
 * The phone's {@code vault} capability: {@code desktop.focus} and {@code vault.type} requests to the desktop
 * (protocol page, "Desktop facts and secret typing"; ADR-0440). The UI thread queues commands; the session
 * thread sends them from {@link #onTick} (a wake follows every command) so the engine's threading rule holds.
 *
 * AGENT-GUARD: bodies are never logged. A {@link VaultTypingSource} is closed right after its frame was
 * encoded for the writer (or dropped), on every path including disconnect.
 * AGENT-CONTRACT: desktop side is src/services/link_vault VaultTypingFeature.
 */
public final class VaultTypeFeature implements FeatureHandler, VaultTypeFlow.DesktopLink {
    private abstract static class Cmd {
        final AtomicBoolean done = new AtomicBoolean();
        abstract void fail(String code);
    }
    private static final class FocusCmd extends Cmd {
        final VaultTypeFlow.FocusCallback cb;
        FocusCmd(VaultTypeFlow.FocusCallback cb) { this.cb = cb; }
        @Override void fail(String code) { if (done.compareAndSet(false, true)) cb.onError(code); }
    }
    private static final class TypeCmd extends Cmd {
        final String token;
        final VaultTypingSource src;
        final VaultTypeFlow.ResultCallback cb;
        TypeCmd(String token, VaultTypingSource src, VaultTypeFlow.ResultCallback cb) { this.token = token; this.src = src; this.cb = cb; }
        @Override void fail(String code) {
            src.close();
            if (done.compareAndSet(false, true)) cb.onError(code);
        }
    }

    private final ConcurrentLinkedQueue<Cmd> queue = new ConcurrentLinkedQueue<>();
    private final ConcurrentLinkedQueue<Cmd> inflight = new ConcurrentLinkedQueue<>();
    private volatile FeatureContext ctx;

    @Override public String capability() { return "vault"; }
    @Override public List<String> namespaces() { return Collections.unmodifiableList(Arrays.asList("vault", "desktop")); }

    @Override public void onReady(FeatureContext c) { ctx = c; if (!queue.isEmpty()) c.wake(); }
    @Override public void onTick(long nowMs) throws IOException { drain(); }
    @Override public boolean onFrame(Envelope e) { return false; }   // replies come through request()

    @Override public void onClosed() {
        ctx = null;
        for (Cmd c; (c = inflight.poll()) != null; ) c.fail("disconnected");
        for (Cmd c; (c = queue.poll()) != null; ) c.fail("disconnected");
    }

    @Override public void requestFocus(VaultTypeFlow.FocusCallback cb) { enqueue(new FocusCmd(cb)); }

    @Override public void sendType(String token, VaultTypingSource src, VaultTypeFlow.ResultCallback cb) {
        enqueue(new TypeCmd(token, src, cb));
    }

    private void enqueue(Cmd c) {
        FeatureContext x = ctx;
        if (x == null) { c.fail("not-connected"); return; }
        queue.add(c);
        x.wake();
    }

    private void drain() throws IOException {
        FeatureContext x = ctx;
        if (x == null) return;
        for (Cmd c; (c = queue.poll()) != null; ) {
            try {
                if (c instanceof FocusCmd) sendFocus(x, (FocusCmd) c); else sendType(x, (TypeCmd) c);
            } catch (IOException | RuntimeException ex) {
                c.fail("disconnected");
                if (ex instanceof IOException) throw (IOException) ex;
            }
        }
    }

    private void sendFocus(FeatureContext x, FocusCmd c) throws IOException {
        inflight.add(c);
        boolean sent = x.request("desktop.focus", new LinkedHashMap<String, Object>(), reply -> {
            inflight.remove(c);
            if (!c.done.compareAndSet(false, true)) return;
            if ("desktop.focus".equals(reply.t) && reply.b.get("token") instanceof String) {
                c.cb.onFocus((String) reply.b.get("token"), str(reply.b.get("app")), str(reply.b.get("title")));
            } else {
                c.cb.onError(errorCode(reply));
            }
        });
        if (!sent) { inflight.remove(c); c.fail("not-connected"); }
    }

    private void sendType(FeatureContext x, TypeCmd c) {
        boolean sent = false;
        try {
            Map<String, Object> body = new LinkedHashMap<>();
            body.put("token", c.token);
            body.put("steps", new java.util.ArrayList<Object>(VaultWireSteps.encode(c.src.steps())));
            inflight.add(c);
            sent = x.request("vault.type", body, reply -> {
                inflight.remove(c);
                if (!c.done.compareAndSet(false, true)) return;
                if ("ok".equals(reply.t)) c.cb.onDone(); else c.cb.onError(errorCode(reply));
            });
            if (!sent) inflight.remove(c);
            body.clear();
        } catch (VaultException e) {
            c.fail("locked");
            return;
        } catch (VaultWireSteps.Unsupported e) {
            c.fail("bad-request");
            return;
        } catch (IOException e) {
            c.fail("disconnected");
            return;
        } finally {
            c.src.close();   // the frame is already encoded for the writer; the text buffers are zeroed here
        }
        if (!sent) c.fail("not-connected");
    }

    private static String str(Object o) { return o instanceof String ? (String) o : ""; }

    private static String errorCode(Envelope reply) {
        return "error".equals(reply.t) && reply.b != null && reply.b.get("code") instanceof String ? (String) reply.b.get("code") : "internal";
    }
}
