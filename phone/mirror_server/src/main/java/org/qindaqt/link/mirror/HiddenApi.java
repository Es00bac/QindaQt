// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.content.Intent;
import android.os.Binder;
import android.os.Bundle;
import android.os.IBinder;
import android.view.InputEvent;
import android.view.MotionEvent;
import android.view.Surface;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

/**
 * Reflection probe for the hidden framework services the mirror server uses as the shell user.
 *
 * <p>AGENT-NOTE: every signature is looked up once at startup so an Android or One UI change
 * fails with the exact missing method instead of a late NoSuchMethodError. The verified
 * shapes on SDK 37 (SM-S948U, One UI) are listed in docs/wiki/reference/qindalink-mirror-v1.md.
 * Required pieces gate the whole session; optional ones degrade a single feature.
 */
final class HiddenApi {
    static final String SHELL_PACKAGE = "com.android.shell";

    final List<String> missing = new ArrayList<>();
    final List<String> notes = new ArrayList<>();

    Object displayManager;
    Class<?> displayCallbackInterface;
    Method createVirtualDisplay;
    Method resizeVirtualDisplay;
    Method setVirtualDisplaySurface;
    Method releaseVirtualDisplay;
    Method getDisplayInfo; // optional: display group of the mirror display (DisplayPower)

    Object inputManager;
    Method injectInputEvent;
    Method setDisplayId;
    Method setActionButton; // optional: mouse button press/release events

    Object activityService;
    Method startActivity;
    boolean startActivityTakesFeature;

    Object packageManager;
    Method queryIntentActivities;
    boolean queryFlagsAreLong;

    Object activityTaskManager;
    Method rootTasksOnDisplay; // optional: app-exit detection

    Object windowManager;
    Method setDisplayImePolicy; // optional: keep the phone keyboard off the mirror display

    Object powerManager;
    Method acquireWakeLock; // optional: per-display screen wake lock (keeps only our display awake)
    Method releaseWakeLock;
    Method isDisplayInteractive; // optional: diagnostics only

    private HiddenApi() {}

    static HiddenApi probe() {
        HiddenApi api = new HiddenApi();
        api.probeDisplay();
        api.probeInput();
        api.probeActivity();
        api.probePackages();
        api.probeWindow();
        api.probePower();
        api.probeTasks();
        return api;
    }

    boolean complete() {
        return missing.isEmpty();
    }

    String report() {
        StringBuilder text = new StringBuilder();
        text.append(complete() ? "hidden API complete" : "hidden API missing: " + missing);
        if (!notes.isEmpty()) {
            text.append("; ").append(String.join("; ", notes));
        }
        return text.toString();
    }

    /** A binder token standing in for IVirtualDisplayCallback; system_server keys the display by it. */
    Object newDisplayCallback() {
        final Binder token = new Binder();
        return Proxy.newProxyInstance(displayCallbackInterface.getClassLoader(),
                new Class<?>[] {displayCallbackInterface}, (proxy, method, args) -> {
                    switch (method.getName()) {
                        case "asBinder":
                            return token;
                        case "hashCode":
                            return System.identityHashCode(proxy);
                        case "equals":
                            return proxy == args[0];
                        case "toString":
                            return "QindaLinkMirrorDisplayCallback";
                        default:
                            return null; // onPaused/onResumed/onStopped need no action
                    }
                });
    }

    /** Unwraps reflective failures so callers see the framework's own exception. */
    static Object call(Method method, Object target, Object... args) throws Exception {
        try {
            return method.invoke(target, args);
        } catch (InvocationTargetException e) {
            Throwable cause = e.getCause();
            if (cause instanceof Exception) {
                throw (Exception) cause;
            }
            throw e;
        }
    }

    private void probeDisplay() {
        displayManager = service("display", "android.hardware.display.IDisplayManager");
        displayCallbackInterface = type("android.hardware.display.IVirtualDisplayCallback");
        Class<?> dm = type("android.hardware.display.IDisplayManager");
        Class<?> config = type("android.hardware.display.VirtualDisplayConfig");
        Class<?> projection = type("android.media.projection.IMediaProjection");
        if (dm == null || displayCallbackInterface == null || config == null || projection == null) {
            return;
        }
        Class<?> cb = displayCallbackInterface;
        createVirtualDisplay = method(dm, "createVirtualDisplay", true, config, cb, projection, String.class);
        resizeVirtualDisplay = method(dm, "resizeVirtualDisplay", true, cb, int.class, int.class, int.class);
        setVirtualDisplaySurface = method(dm, "setVirtualDisplaySurface", true, cb, Surface.class);
        releaseVirtualDisplay = method(dm, "releaseVirtualDisplay", true, cb);
        getDisplayInfo = optional(dm, "getDisplayInfo", int.class);
    }

    private void probeInput() {
        inputManager = service("input", "android.hardware.input.IInputManager");
        Class<?> im = type("android.hardware.input.IInputManager");
        if (im != null) {
            injectInputEvent = method(im, "injectInputEvent", true, InputEvent.class, int.class);
        }
        setDisplayId = method(InputEvent.class, "setDisplayId", true, int.class);
        setActionButton = method(MotionEvent.class, "setActionButton", false, int.class);
    }

    private void probeActivity() {
        Class<?> thread = type("android.app.IApplicationThread");
        Class<?> profiler = type("android.app.ProfilerInfo");
        if (thread == null || profiler == null) {
            return;
        }
        Object am = service("activity", "android.app.IActivityManager");
        Class<?> amType = type("android.app.IActivityManager");
        if (am != null && amType != null) {
            Method withFeature = optional(amType, "startActivityAsUserWithFeature", thread, String.class,
                    String.class, Intent.class, String.class, IBinder.class, String.class, int.class,
                    int.class, profiler, Bundle.class, int.class);
            if (withFeature != null) {
                activityService = am;
                startActivity = withFeature;
                startActivityTakesFeature = true;
                return;
            }
            Method plain = optional(amType, "startActivityAsUser", thread, String.class, Intent.class,
                    String.class, IBinder.class, String.class, int.class, int.class, profiler,
                    Bundle.class, int.class);
            if (plain != null) {
                activityService = am;
                startActivity = plain;
                notes.add("using IActivityManager.startActivityAsUser without feature id");
                return;
            }
        }
        missing.add("IActivityManager.startActivityAsUserWithFeature(IApplicationThread, String, String,"
                + " Intent, String, IBinder, String, int, int, ProfilerInfo, Bundle, int)");
    }

    private void probePackages() {
        packageManager = service("package", "android.content.pm.IPackageManager");
        Class<?> pm = type("android.content.pm.IPackageManager");
        if (pm == null) {
            return;
        }
        queryIntentActivities = optional(pm, "queryIntentActivities", Intent.class, String.class,
                long.class, int.class);
        queryFlagsAreLong = queryIntentActivities != null;
        if (queryIntentActivities == null) {
            queryIntentActivities = method(pm, "queryIntentActivities", true, Intent.class,
                    String.class, int.class, int.class);
        }
    }

    private void probeWindow() {
        windowManager = serviceOptional("window", "android.view.IWindowManager");
        Class<?> wm = type("android.view.IWindowManager");
        if (wm != null && windowManager != null) {
            setDisplayImePolicy = method(wm, "setDisplayImePolicy", false, int.class, int.class);
        }
    }

    private void probePower() {
        powerManager = serviceOptional("power", "android.os.IPowerManager");
        Class<?> pm = optionalType("android.os.IPowerManager");
        Class<?> callback = optionalType("android.os.IWakeLockCallback");
        if (pm == null || powerManager == null) {
            notes.add("optional IPowerManager unavailable");
            return;
        }
        if (callback != null) {
            acquireWakeLock = method(pm, "acquireWakeLock", false, IBinder.class, int.class, String.class,
                    String.class, android.os.WorkSource.class, String.class, int.class, callback);
        }
        releaseWakeLock = method(pm, "releaseWakeLock", false, IBinder.class, int.class);
        isDisplayInteractive = optional(pm, "isDisplayInteractive", int.class);
    }

    private void probeTasks() {
        activityTaskManager = serviceOptional("activity_task", "android.app.IActivityTaskManager");
        Class<?> atm = optionalType("android.app.IActivityTaskManager");
        if (atm != null && activityTaskManager != null) {
            rootTasksOnDisplay = method(atm, "getAllRootTaskInfosOnDisplay", false, int.class);
        }
    }

    private Class<?> optionalType(String name) {
        try {
            return Class.forName(name);
        } catch (ClassNotFoundException e) {
            return null;
        }
    }

    private Object service(String name, String iface) {
        Object binder = serviceOptional(name, iface);
        if (binder == null) {
            missing.add("service '" + name + "' as " + iface);
        }
        return binder;
    }

    private Object serviceOptional(String name, String iface) {
        try {
            Class<?> sm = Class.forName("android.os.ServiceManager");
            IBinder binder = (IBinder) sm.getMethod("getService", String.class).invoke(null, name);
            if (binder == null) {
                return null;
            }
            return Class.forName(iface + "$Stub").getMethod("asInterface", IBinder.class).invoke(null, binder);
        } catch (ReflectiveOperationException | RuntimeException e) {
            notes.add("service " + name + ": " + Ln.describe(e));
            return null;
        }
    }

    private Class<?> type(String name) {
        try {
            return Class.forName(name);
        } catch (ClassNotFoundException e) {
            missing.add("class " + name);
            return null;
        }
    }

    private Method method(Class<?> owner, String name, boolean required, Class<?>... params) {
        Method found = optional(owner, name, params);
        if (found == null) {
            String signature = owner.getSimpleName() + "." + name + signature(params);
            if (required) {
                missing.add(signature);
            } else {
                notes.add("optional " + signature + " unavailable");
            }
        }
        return found;
    }

    private static Method optional(Class<?> owner, String name, Class<?>... params) {
        try {
            Method found = owner.getDeclaredMethod(name, params);
            found.setAccessible(true);
            return found;
        } catch (NoSuchMethodException e) {
            try {
                return owner.getMethod(name, params);
            } catch (NoSuchMethodException again) {
                return null;
            }
        }
    }

    private static String signature(Class<?>... params) {
        StringBuilder text = new StringBuilder("(");
        for (int i = 0; i < params.length; i++) {
            text.append(i == 0 ? "" : ", ").append(params[i].getSimpleName());
        }
        return text.append(')').toString();
    }
}
