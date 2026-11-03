package org.qindaqt.link.engine;

/** Values for the phone's hello frame. */
public final class DeviceInfo {
    public final String name, model, os, app;

    public DeviceInfo(String name, String model, String os, String app) {
        this.name = clip(name, 128);
        this.model = clip(model, 128);
        this.os = clip(os, 128);
        this.app = clip(app, 64);
    }

    /**
     * The user-visible device name: the Settings.Global device name when set, else the model.
     * Pure so the JVM tests can cover the fallback; the Android caller supplies the setting.
     */
    public static String displayName(String settingsName, String model) {
        String n = settingsName == null ? "" : settingsName.trim();
        return n.isEmpty() ? model : n;
    }

    private static String clip(String s, int n) { return s.length() <= n ? s : s.substring(0, n); }
}
