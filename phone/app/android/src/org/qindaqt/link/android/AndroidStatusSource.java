package org.qindaqt.link.android;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.wifi.WifiManager;
import android.telephony.SignalStrength;
import android.telephony.TelephonyCallback;
import android.telephony.TelephonyManager;
import android.os.BatteryManager;
import java.util.LinkedHashMap;
import java.util.Map;
import org.qindaqt.link.engine.StatusSource;
import org.qindaqt.link.proto.Json;

/**
 * Battery and network facts for the status frame. Reads the sticky battery
 * broadcast (no receiver registration, no permission) and the active
 * network's transport plus a 0-4 signal level: Wi-Fi from the network
 * capabilities' RSSI (the RSSI is not location-redacted, unlike SSID/BSSID),
 * cellular from TelephonyManager.getSignalStrength().getLevel(). Any failure
 * (no permission, no radio, unspecified value) yields null, never a guess.
 */
final class AndroidStatusSource implements StatusSource {
    private final Context ctx;
    private volatile Map<String, Object> cached;
    private volatile Runnable listener;
    private volatile String netType = "none";
    private volatile Object signal = Json.NULL;
    private TelephonyManager tm;
    private TelephonyCallback telCb;
    private ConnectivityManager cm;
    private ConnectivityManager.NetworkCallback netCb;
    private BroadcastReceiver batteryRx;

    AndroidStatusSource(Context ctx) { this.ctx = ctx; }

    /**
     * Registers battery and default-network callbacks. The snapshot is rebuilt
     * only when one fires, so the session's tick never touches system services.
     * Must be paired with {@link #stop()}.
     */
    void start() {
        cm = (ConnectivityManager) ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
        netCb = new ConnectivityManager.NetworkCallback() {
            @Override public void onCapabilitiesChanged(Network n, NetworkCapabilities c) { netType = typeOf(c); signal = signalOf(c); refresh(); }
            @Override public void onLost(Network n) { netType = "none"; signal = Json.NULL; refresh(); }
        };
        cm.registerDefaultNetworkCallback(netCb);
        batteryRx = new BroadcastReceiver() {
            @Override public void onReceive(Context c, Intent i) { refresh(); }
        };
        ctx.registerReceiver(batteryRx, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        try {
            tm = ctx.getSystemService(TelephonyManager.class);
            telCb = new SignalCb();
            tm.registerTelephonyCallback(ctx.getMainExecutor(), (TelephonyCallback) telCb);
        } catch (RuntimeException e) {
            tm = null; // no telephony or no permission: cellular signal stays unknown
        }
        refresh();
    }

    /** Cellular level changes arrive here; the snapshot is rebuilt only when the level really moved. */
    private final class SignalCb extends TelephonyCallback implements TelephonyCallback.SignalStrengthsListener {
        @Override public void onSignalStrengthsChanged(SignalStrength s) {
            if ("cellular".equals(netType)) { signal = level(s.getLevel()); refresh(); }
        }
    }

    void stop() {
        try { if (tm != null && telCb != null) tm.unregisterTelephonyCallback(telCb); } catch (RuntimeException ignored) { /* already gone */ }
        try { if (cm != null && netCb != null) cm.unregisterNetworkCallback(netCb); } catch (RuntimeException ignored) { /* already gone */ }
        try { if (batteryRx != null) ctx.unregisterReceiver(batteryRx); } catch (RuntimeException ignored) { /* already gone */ }
    }

    @Override public void setListener(Runnable r) { listener = r; }

    private void refresh() {
        cached = build();
        Runnable r = listener;
        if (r != null) r.run();
    }

    @Override public Map<String, Object> snapshot() {
        Map<String, Object> c = cached;
        return c != null ? c : build();
    }

    private Map<String, Object> build() {
        Map<String, Object> bat = new LinkedHashMap<>();
        Intent i = ctx.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        int level = 0, plug = 0, st = -1;
        if (i != null) {
            int l = i.getIntExtra(BatteryManager.EXTRA_LEVEL, 0), sc = i.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
            level = sc > 0 ? Math.max(0, Math.min(100, (int) Math.round(100.0 * l / sc))) : 0;
            plug = i.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0);
            st = i.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
        }
        String plugged = plug == 0 ? "none" : (plug & BatteryManager.BATTERY_PLUGGED_USB) != 0 ? "usb"
                : (plug & BatteryManager.BATTERY_PLUGGED_AC) != 0 ? "ac"
                : (plug & BatteryManager.BATTERY_PLUGGED_WIRELESS) != 0 ? "wireless"
                : (plug & BatteryManager.BATTERY_PLUGGED_DOCK) != 0 ? "dock" : "none";
        bat.put("level", (long) level);
        bat.put("charging", st == BatteryManager.BATTERY_STATUS_CHARGING || st == BatteryManager.BATTERY_STATUS_FULL && plug != 0);
        bat.put("plugged", plugged);

        Map<String, Object> net = new LinkedHashMap<>();
        net.put("type", netType);
        net.put("signal", signal);

        Map<String, Object> body = new LinkedHashMap<>();
        body.put("battery", bat);
        body.put("network", net);
        return body;
    }

    /** 0-4 for the active transport, or JSON null when the platform cannot say. */
    private Object signalOf(NetworkCapabilities c) {
        try {
            if (c == null) return Json.NULL;
            if (c.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) {
                int rssi = c.getSignalStrength();
                if (rssi == NetworkCapabilities.SIGNAL_STRENGTH_UNSPECIFIED) return Json.NULL;
                WifiManager wm = ctx.getSystemService(WifiManager.class);
                return wm == null ? Json.NULL : level(wm.calculateSignalLevel(rssi));
            }
            if (c.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) && tm != null) {
                SignalStrength s = tm.getSignalStrength();
                return s == null ? Json.NULL : level(s.getLevel());
            }
        } catch (RuntimeException e) {
            return Json.NULL; // SecurityException or a vendor quirk: unknown, not zero
        }
        return Json.NULL;
    }

    private static Object level(int l) { return (l < 0 || l > 4) ? Json.NULL : (Object) Long.valueOf(l); }

    private static String typeOf(NetworkCapabilities c) {
        if (c == null) return "none";
        if (c.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return "wifi";
        if (c.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) return "cellular";
        if (c.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) return "ethernet";
        return "other";
    }
}
