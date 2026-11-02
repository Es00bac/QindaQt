package org.qindaqt.link.android;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.PowerManager;
import android.provider.Settings;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Read-only facts about this phone for the UI: battery, network and the state of each
 * permission the Permissions screen lists, plus the Android settings pages that fix them.
 *
 * AGENT-CONTRACT: the JSON keys below are read by phone/app/LinkBridge.cpp (deviceFacts) and by
 * the preview host's FakeBridge. Permission states are "granted", "denied" or "later"; "later"
 * means this build does not declare or implement the feature, so the screen shows "coming soon"
 * instead of a fake value. A permission turns from "later" to a real state by itself once the
 * owning lane declares it in the manifest (or adds the NotificationListenerService).
 */
public final class DeviceFacts {
    private DeviceFacts() {}

    public static String json(Context ctx) {
        StringBuilder b = new StringBuilder("{");
        Intent bi = ctx.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        int level = -1;
        boolean charging = false;
        if (bi != null) {
            int l = bi.getIntExtra(BatteryManager.EXTRA_LEVEL, -1), sc = bi.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
            if (l >= 0 && sc > 0) level = Math.round(100f * l / sc);
            charging = bi.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0) != 0;
        }
        b.append("\"batteryLevel\":").append(level).append(",\"charging\":").append(charging);
        b.append(",\"network\":\"").append(network(ctx)).append('"');
        b.append(",\"perm\":{");
        b.append("\"notificationAccess\":\"").append(notificationAccess(ctx)).append('"');
        b.append(",\"sms\":\"").append(runtime(ctx, "android.permission.READ_SMS")).append('"');
        b.append(",\"contacts\":\"").append(runtime(ctx, "android.permission.READ_CONTACTS")).append('"');
        b.append(",\"postNotifications\":\"").append(runtime(ctx, "android.permission.POST_NOTIFICATIONS")).append('"');
        b.append(",\"batteryOptimisation\":\"").append(battery(ctx)).append('"');
        return b.append("}}").toString();
    }

    /** Opens the settings page for a permission key; unknown keys are ignored. */
    public static void openSettings(Context ctx, String key) {
        Intent i;
        switch (key) {
            case "notificationAccess": i = new Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS); break;
            case "batteryOptimisation": i = new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS); break;
            case "sms": case "contacts": case "postNotifications":
                i = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:" + ctx.getPackageName()));
                break;
            default: return;
        }
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        try { ctx.startActivity(i); } catch (RuntimeException ignored) { /* no settings page on this ROM */ }
    }

    private static String network(Context ctx) {
        ConnectivityManager cm = (ConnectivityManager) ctx.getSystemService(Context.CONNECTIVITY_SERVICE);
        Network n = cm == null ? null : cm.getActiveNetwork();
        NetworkCapabilities c = n == null ? null : cm.getNetworkCapabilities(n);
        if (c == null) return "none";
        if (c.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return "wifi";
        if (c.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)) return "cellular";
        return "other";
    }

    private static Set<String> declared(Context ctx) {
        try {
            PackageInfo pi = ctx.getPackageManager().getPackageInfo(ctx.getPackageName(), PackageManager.GET_PERMISSIONS);
            return pi.requestedPermissions == null ? new HashSet<>() : new HashSet<>(Arrays.asList(pi.requestedPermissions));
        } catch (PackageManager.NameNotFoundException e) {
            return new HashSet<>();
        }
    }

    private static String runtime(Context ctx, String perm) {
        if (!declared(ctx).contains(perm)) return "later";
        return ctx.checkSelfPermission(perm) == PackageManager.PERMISSION_GRANTED ? "granted" : "denied";
    }

    private static String notificationAccess(Context ctx) {
        Intent probe = new Intent("android.service.notification.NotificationListenerService").setPackage(ctx.getPackageName());
        if (ctx.getPackageManager().queryIntentServices(probe, 0).isEmpty()) return "later";
        String enabled = Settings.Secure.getString(ctx.getContentResolver(), "enabled_notification_listeners");
        return enabled != null && enabled.contains(ctx.getPackageName() + "/") ? "granted" : "denied";
    }

    private static String battery(Context ctx) {
        PowerManager pm = (PowerManager) ctx.getSystemService(Context.POWER_SERVICE);
        return pm != null && pm.isIgnoringBatteryOptimizations(ctx.getPackageName()) ? "granted" : "denied";
    }
}
