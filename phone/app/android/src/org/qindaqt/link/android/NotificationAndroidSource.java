package org.qindaqt.link.android;

import android.app.Notification;
import android.app.NotificationManager;
import android.app.RemoteInput;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;
import java.io.ByteArrayOutputStream;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.qindaqt.link.notify.NotificationFilter;
import org.qindaqt.link.notify.NotificationSource;
import org.qindaqt.link.notify.NotifyFrames;
import org.qindaqt.link.notify.PhoneNotification;

/**
 * Android side of the notify capability: converts StatusBarNotifications into
 * {@link PhoneNotification}s, renders app icons (64 px PNG, SHA-256) and fires actions.
 *
 * AGENT-NOTE: a singleton because the system instantiates {@link NotificationListener} itself and
 * LinkService creates the feature independently; they meet here. Access is "granted" only while the
 * listener service is bound, since an unbound listener cannot dismiss or act.
 * AGENT-GUARD: notification text, titles and remote-input text never reach logs from this class.
 * Android 15+ may hand us redacted text for sensitive notifications; it is forwarded as received.
 */
final class NotificationAndroidSource implements NotificationSource {
    static final NotificationAndroidSource INSTANCE = new NotificationAndroidSource();
    private static final int ICON_PX = 64;
    private static final int ICON_CACHE = 128;

    private volatile NotificationListener service;
    private volatile Listener listener;
    private Context appContext;
    private final Map<String, String> hashByPackage = new LinkedHashMap<>();
    private final Map<String, byte[]> pngByHash = new LinkedHashMap<String, byte[]>() {
        private static final long serialVersionUID = 1L;
        @Override protected boolean removeEldestEntry(Map.Entry<String, byte[]> e) { return size() > ICON_CACHE; }
    };

    private NotificationAndroidSource() { }

    /** Called by LinkService. Asks Android to rebind the listener when access is granted but it is not bound. */
    static NotificationAndroidSource init(Context ctx) {
        INSTANCE.appContext = ctx.getApplicationContext();
        try {
            ComponentName cn = new ComponentName(ctx, NotificationListener.class);
            NotificationManager nm = ctx.getSystemService(NotificationManager.class);
            if (INSTANCE.service == null && nm.isNotificationListenerAccessGranted(cn)) {
                NotificationListenerService.requestRebind(cn);
            }
        } catch (RuntimeException ignored) { /* access state stays "missing" */ }
        return INSTANCE;
    }

    void connected(NotificationListener s) {
        service = s;
        Listener l = listener;
        if (l != null) l.onAccessChanged(true);
    }

    void disconnected(NotificationListener s) {
        if (service == s) service = null;
        Listener l = listener;
        if (l != null) l.onAccessChanged(false);
    }

    void posted(StatusBarNotification sbn) {
        Listener l = listener;
        if (l == null) return;
        PhoneNotification n = convert(sbn);
        if (n != null) l.onPosted(n);
    }

    void removed(StatusBarNotification sbn) {
        Listener l = listener;
        if (l != null && mirrored(sbn)) l.onRemoved(sbn.getKey());
    }

    @Override public void setListener(Listener l) { listener = l; }
    @Override public boolean accessGranted() { return service != null; }

    @Override public List<PhoneNotification> active() {
        NotificationListener s = service;
        List<PhoneNotification> out = new ArrayList<>();
        if (s == null) return out;
        try {
            StatusBarNotification[] all = s.getActiveNotifications();
            if (all != null) for (StatusBarNotification sbn : all) {
                PhoneNotification n = convert(sbn);
                if (n != null) out.add(n);
            }
        } catch (RuntimeException ignored) { /* listener went away mid-call */ }
        return out;
    }

    @Override public boolean dismiss(String key) {
        NotificationListener s = service;
        if (s == null || find(s, key) == null) return false;
        s.cancelNotification(key);
        return true;
    }

    @Override public String perform(String key, String actionId, String replyText) {
        NotificationListener s = service;
        if (s == null) return "denied";
        StatusBarNotification sbn = find(s, key);
        if (sbn == null) return "target-changed";
        Notification.Action[] acts = sbn.getNotification().actions;
        int idx;
        try { idx = Integer.parseInt(actionId); } catch (NumberFormatException e) { return "bad-request"; }
        if (acts == null || idx < 0 || idx >= acts.length || acts[idx].actionIntent == null) return "bad-request";
        Notification.Action act = acts[idx];
        boolean reply = isReply(act);
        if (reply != (replyText != null)) return "bad-request";
        Intent fill = new Intent();
        if (reply) {
            RemoteInput[] inputs = act.getRemoteInputs();
            Bundle results = new Bundle();
            for (RemoteInput ri : inputs) if (ri.getAllowFreeFormInput()) results.putCharSequence(ri.getResultKey(), replyText);
            RemoteInput.addResultsToIntent(inputs, fill, results);
        }
        try {
            act.actionIntent.send(s, 0, fill);
            return null;
        } catch (android.app.PendingIntent.CanceledException e) {
            return "target-changed";
        } catch (RuntimeException e) {
            return "internal";  // for example a background activity launch blocked by Android
        }
    }

    @Override public synchronized byte[] icon(String hash) { return pngByHash.get(hash); }

    private static StatusBarNotification find(NotificationListener s, String key) {
        try {
            StatusBarNotification[] r = s.getActiveNotifications(new String[] {key});
            return r != null && r.length > 0 ? r[0] : null;
        } catch (RuntimeException e) {
            return null;
        }
    }

    private static boolean isReply(Notification.Action a) {
        RemoteInput[] inputs = a.getRemoteInputs();
        if (inputs != null) for (RemoteInput ri : inputs) if (ri.getAllowFreeFormInput()) return true;
        return false;
    }

    private boolean mirrored(StatusBarNotification sbn) {
        int f = sbn.getNotification().flags;
        return NotificationFilter.mirror((f & Notification.FLAG_ONGOING_EVENT) != 0,
                (f & Notification.FLAG_FOREGROUND_SERVICE) != 0, (f & Notification.FLAG_GROUP_SUMMARY) != 0,
                sbn.getPackageName(), appContext == null ? "" : appContext.getPackageName());
    }

    private PhoneNotification convert(StatusBarNotification sbn) {
        if (!mirrored(sbn)) return null;
        Notification n = sbn.getNotification();
        Bundle x = n.extras;
        String pkg = sbn.getPackageName();
        String title = str(x.getCharSequence(Notification.EXTRA_TITLE));
        String text = str(x.getCharSequence(Notification.EXTRA_BIG_TEXT));
        if (text.isEmpty()) text = str(x.getCharSequence(Notification.EXTRA_TEXT));
        List<PhoneNotification.Action> actions = new ArrayList<>();
        if (n.actions != null) for (int i = 0; i < n.actions.length; i++) {
            Notification.Action a = n.actions[i];
            if (a.actionIntent == null || a.title == null) continue;
            actions.add(new PhoneNotification.Action(Integer.toString(i), a.title.toString(), isReply(a)));
        }
        return new PhoneNotification(sbn.getKey(), pkg, label(pkg), title, text, sbn.getPostTime(), iconHash(pkg), actions);
    }

    private static String str(CharSequence cs) { return cs == null ? "" : cs.toString(); }

    private String label(String pkg) {
        try {
            PackageManager pm = appContext.getPackageManager();
            return pm.getApplicationLabel(pm.getApplicationInfo(pkg, 0)).toString();
        } catch (Exception e) {
            return pkg;
        }
    }

    /** SHA-256 of the 64 px PNG of the app icon, remembering the PNG for notify.icon requests; "" on failure. */
    private synchronized String iconHash(String pkg) {
        String known = hashByPackage.get(pkg);
        if (known != null) return known;
        String hash = "";
        try {
            Drawable d = appContext.getPackageManager().getApplicationIcon(pkg);
            Bitmap bmp = Bitmap.createBitmap(ICON_PX, ICON_PX, Bitmap.Config.ARGB_8888);
            d.setBounds(0, 0, ICON_PX, ICON_PX);
            d.draw(new Canvas(bmp));
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            bmp.compress(Bitmap.CompressFormat.PNG, 100, out);
            byte[] png = out.toByteArray();
            if (png.length > 0 && png.length * 4 / 3 + 4 <= NotifyFrames.ICON_BASE64) {
                StringBuilder sb = new StringBuilder();
                for (byte b : MessageDigest.getInstance("SHA-256").digest(png)) sb.append(String.format("%02x", b));
                hash = sb.toString();
                pngByHash.put(hash, png);
            }
        } catch (Exception ignored) { /* no icon: the desktop shows the notification without one */ }
        if (hashByPackage.size() > ICON_CACHE) hashByPackage.clear();
        hashByPackage.put(pkg, hash);
        return hash;
    }
}
