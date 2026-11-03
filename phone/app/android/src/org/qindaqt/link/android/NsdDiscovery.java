package org.qindaqt.link.android;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.os.Build;
import android.util.Log;
import java.net.Inet4Address;
import java.net.InetAddress;
import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executor;
import org.qindaqt.link.engine.DesktopDiscovery;
import org.qindaqt.link.engine.DiscoveredDesktop;
import org.qindaqt.link.engine.DiscoveryTable;

/**
 * mDNS discovery of _qindalink._tcp through the system NsdManager (no multicast lock needed: the
 * system mDNS daemon does the sockets). Results go to a {@link DiscoveryTable}; the TXT filter
 * lives in {@link DiscoveredDesktop#fromTxt}.
 *
 * AGENT-NOTE: Android 14+ resolves through registerServiceInfoCallback, which tracks address changes;
 * Android 12-13 (minSdk 31) use resolveService, which allows only one resolve at a time, so requests
 * are queued. Android 17 enforces the local-network permission (ACCESS_LOCAL_NETWORK) for apps that
 * target API 37; this build targets 36 and declares the permission in the manifest for when that changes.
 * AGENT-GUARD: never log service names, addresses or TXT values; they identify the user's network.
 */
final class NsdDiscovery implements DesktopDiscovery {
    private static final String TAG = "QindaLink";
    private static final String TYPE = "_qindalink._tcp";

    private final NsdManager nsd;
    private final Executor direct = Runnable::run;
    private DiscoveryTable sink;
    private NsdManager.DiscoveryListener listener;
    private final Map<String, NsdManager.ServiceInfoCallback> callbacks = new HashMap<>();
    private final ArrayDeque<NsdServiceInfo> resolveQueue = new ArrayDeque<>();
    private boolean resolving;

    NsdDiscovery(Context ctx) {
        nsd = ctx.getSystemService(NsdManager.class);
    }

    @Override public synchronized void start(DiscoveryTable table) {
        if (listener != null || nsd == null) return;
        sink = table;
        listener = new NsdManager.DiscoveryListener() {
            @Override public void onDiscoveryStarted(String type) { }
            @Override public void onDiscoveryStopped(String type) { }
            @Override public void onStartDiscoveryFailed(String type, int code) {
                Log.w(TAG, "nsd start failed: " + code);
                synchronized (NsdDiscovery.this) { if (listener == this) listener = null; }
            }
            @Override public void onStopDiscoveryFailed(String type, int code) { }
            @Override public void onServiceFound(NsdServiceInfo info) { resolve(info); }
            @Override public void onServiceLost(NsdServiceInfo info) { forget(info); }
        };
        try {
            nsd.discoverServices(TYPE, NsdManager.PROTOCOL_DNS_SD, listener);
        } catch (RuntimeException e) {
            Log.w(TAG, "nsd refused: " + e.getClass().getSimpleName());
            listener = null;
        }
    }

    @Override public synchronized void stop() {
        if (listener == null || nsd == null) return;
        try { nsd.stopServiceDiscovery(listener); } catch (RuntimeException ignored) { /* already stopped */ }
        listener = null;
        if (Build.VERSION.SDK_INT >= 34) {
            for (NsdManager.ServiceInfoCallback cb : callbacks.values()) {
                try { nsd.unregisterServiceInfoCallback(cb); } catch (RuntimeException ignored) { /* gone */ }
            }
        }
        callbacks.clear();
        resolveQueue.clear();
        sink = null;
    }

    private synchronized void resolve(NsdServiceInfo found) {
        if (listener == null) return;
        if (Build.VERSION.SDK_INT >= 34) {
            final String key = found.getServiceName();
            if (callbacks.containsKey(key)) return;
            NsdManager.ServiceInfoCallback cb = new NsdManager.ServiceInfoCallback() {
                @Override public void onServiceInfoCallbackRegistrationFailed(int code) { synchronized (NsdDiscovery.this) { callbacks.remove(key); } }
                @Override public void onServiceUpdated(NsdServiceInfo info) { publish(info.getServiceName(), addresses(info), info); }
                @Override public void onServiceLost() { DiscoveryTable t = table(); if (t != null) t.lost(key); }
                @Override public void onServiceInfoCallbackUnregistered() { }
            };
            callbacks.put(key, cb);
            nsd.registerServiceInfoCallback(found, direct, cb);
        } else {
            resolveQueue.add(found);
            drainResolveQueue();
        }
    }

    @SuppressWarnings("deprecation")
    private synchronized void drainResolveQueue() {
        if (resolving || resolveQueue.isEmpty()) return;
        NsdServiceInfo next = resolveQueue.poll();
        resolving = true;
        nsd.resolveService(next, new NsdManager.ResolveListener() {
            @Override public void onResolveFailed(NsdServiceInfo info, int code) { done(); }
            @Override public void onServiceResolved(NsdServiceInfo info) {
                InetAddress h = info.getHost();
                publish(info.getServiceName(), h == null ? java.util.Collections.<InetAddress>emptyList() : java.util.Collections.singletonList(h), info);
                done();
            }
            private void done() { synchronized (NsdDiscovery.this) { resolving = false; } drainResolveQueue(); }
        });
    }

    private synchronized void forget(NsdServiceInfo info) {
        NsdManager.ServiceInfoCallback cb = callbacks.remove(info.getServiceName());
        if (cb != null && Build.VERSION.SDK_INT >= 34) {
            try { nsd.unregisterServiceInfoCallback(cb); } catch (RuntimeException ignored) { /* gone */ }
        }
        if (sink != null) sink.lost(info.getServiceName());
    }

    private synchronized DiscoveryTable table() { return sink; }

    private static List<InetAddress> addresses(NsdServiceInfo info) {
        return Build.VERSION.SDK_INT >= 34 ? info.getHostAddresses() : java.util.Collections.<InetAddress>emptyList();
    }

    /** Picks the best address (IPv4 first, never link-local) and hands the TXT-checked desktop to the table. */
    private void publish(String key, List<InetAddress> addrs, NsdServiceInfo info) {
        DiscoveryTable t = table();
        if (t == null) return;
        InetAddress best = null;
        for (InetAddress a : addrs) {
            if (a.isLinkLocalAddress() || a.isLoopbackAddress() || a.isAnyLocalAddress()) continue;
            if (best == null || (a instanceof Inet4Address && !(best instanceof Inet4Address))) best = a;
        }
        if (best == null) return;
        Map<String, String> txt = new HashMap<>();
        Map<String, byte[]> attrs = info.getAttributes();
        if (attrs != null) {
            for (Map.Entry<String, byte[]> e : attrs.entrySet()) {
                if (e.getValue() != null && e.getValue().length <= 256) txt.put(e.getKey(), new String(e.getValue(), StandardCharsets.UTF_8));
            }
        }
        DiscoveredDesktop d = DiscoveredDesktop.fromTxt(key, best.getHostAddress(), info.getPort(), txt);
        if (d != null) t.found(key, d);
    }
}
