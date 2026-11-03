// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.os.Looper;
import android.util.Base64;
import java.io.ByteArrayOutputStream;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Lists launchable apps with labels and PNG icons.
 *
 * <p>AGENT-NOTE: labels and icons live in other packages' resources, which only a Context can
 * load. app_process has none, so this builds the framework system context through a private
 * ActivityThread. That needs a Looper on the calling thread; the server prepares the main
 * looper and calls this only from the session (main) thread.
 *
 * <p>Privacy: the inventory goes only to the desktop that holds the session token. The server
 * logs counts, never labels or package names.
 */
final class AppInventory {
    interface Sink {
        void onApp(JSONObject app) throws java.io.IOException;
    }

    private static final int MIN_ICON = 16;
    private static final int MAX_ICON = 512;
    private Context context;

    /** Streams one JSON object per launchable package; returns the count sent. */
    int list(Set<String> onlyPackages, int iconSize, Sink sink) throws Exception {
        PackageManager pm = context().getPackageManager();
        Intent query = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER);
        List<ResolveInfo> activities = pm.queryIntentActivities(query, 0);
        Set<String> seen = new HashSet<>();
        int size = Math.max(MIN_ICON, Math.min(MAX_ICON, iconSize));
        int sent = 0;
        for (ResolveInfo info : activities) {
            String pkg = info.activityInfo.packageName;
            if ((onlyPackages != null && !onlyPackages.contains(pkg)) || !seen.add(pkg)) {
                continue;
            }
            sink.onApp(describe(pm, info, size));
            sent++;
        }
        Ln.d("inventory sent " + sent + " apps");
        return sent;
    }

    private JSONObject describe(PackageManager pm, ResolveInfo info, int iconSize) throws JSONException {
        JSONObject app = new JSONObject();
        app.put("package", info.activityInfo.packageName);
        app.put("activity", info.activityInfo.name);
        CharSequence label = info.loadLabel(pm);
        app.put("label", label == null ? info.activityInfo.packageName : label.toString());
        try {
            String png = iconPng(info.loadIcon(pm), iconSize);
            if (png != null) {
                app.put("png", png);
            }
        } catch (RuntimeException e) {
            Ln.w("icon render failed: " + Ln.describe(e));
        }
        return app;
    }

    private static String iconPng(Drawable icon, int size) {
        if (icon == null) {
            return null;
        }
        Bitmap bitmap = Bitmap.createBitmap(size, size, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        icon.setBounds(0, 0, size, size);
        icon.draw(canvas);
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        bitmap.recycle();
        return Base64.encodeToString(out.toByteArray(), Base64.NO_WRAP);
    }

    private Context context() throws Exception {
        if (context != null) {
            return context;
        }
        if (Looper.myLooper() == null) {
            throw new IllegalStateException("AppInventory needs a Looper on the calling thread");
        }
        Class<?> threadClass = Class.forName("android.app.ActivityThread");
        Constructor<?> constructor = threadClass.getDeclaredConstructor();
        constructor.setAccessible(true);
        Object thread = constructor.newInstance();
        Field current = threadClass.getDeclaredField("sCurrentActivityThread");
        current.setAccessible(true);
        current.set(null, thread);
        attachConfigurationController(threadClass, thread);
        context = (Context) threadClass.getMethod("getSystemContext").invoke(thread);
        return context;
    }

    /**
     * AGENT-NOTE: since Android 12 ActivityThread.attach() creates its ConfigurationController;
     * a bare ActivityThread has none and getSystemContext() throws an NPE while loading
     * resources (verified on SDK 37). Older releases have no such field and need nothing.
     */
    private static void attachConfigurationController(Class<?> threadClass, Object thread) throws Exception {
        Field field;
        try {
            field = threadClass.getDeclaredField("mConfigurationController");
        } catch (NoSuchFieldException e) {
            return;
        }
        field.setAccessible(true);
        if (field.get(thread) != null) {
            return;
        }
        Class<?> controller = Class.forName("android.app.ConfigurationController");
        Constructor<?> make = controller.getDeclaredConstructor(Class.forName("android.app.ActivityThreadInternal"));
        make.setAccessible(true);
        field.set(thread, make.newInstance(thread));
    }
}
