package org.qindaqt.link.android;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.os.BatteryManager;
import java.util.LinkedHashMap;
import java.util.Map;
import org.qindaqt.link.engine.StatusSource;
import org.qindaqt.link.proto.Json;

/**
 * Battery and network facts for the status frame. Reads the sticky battery
 * broadcast (no receiver registration, no permission) and the active
 * network's transport. Signal strength is always null for now: Wi-Fi RSSI
 * needs location permission and cellular needs READ_PHONE_STATE, neither of
 * which this lane may request.
 */
final class AndroidStatusSource implements StatusSource {
    private final Context ctx;

    AndroidStatusSource(Context ctx) { this.ctx = ctx; }

    @Override public Map<String, Object> snapshot() {
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
        net.put("type", networkType());
        net.put("signal", Json.NULL);

        Map<String, Object> body = new LinkedHashMap<>();
        body.put("battery", bat);
        body.put("network", net);
        return body;
    }

    private String networkType() {
        ConnectivityManager cm = (ConnectivityManager) ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
        Network n = cm == null ? null : cm.getActiveNetwork();
        NetworkCapabilities c = n == null ? null : cm.getNetworkCapabilities(n);
        if (c == null) return "none";
        if (c.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return "wifi";
        if (c.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) return "cellular";
        if (c.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET)) return "ethernet";
        return "other";
    }
}
