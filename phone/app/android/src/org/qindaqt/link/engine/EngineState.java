package org.qindaqt.link.engine;

import java.util.LinkedHashMap;
import java.util.Map;
import org.qindaqt.link.proto.Json;

/**
 * Observable engine state read by the UI bridge. Thread-safe snapshot holder.
 *
 * AGENT-CONTRACT: the JSON from {@link #toJson()} is parsed by the C++ bridge
 * (phone/app/LinkBridge.cpp) and read by the QML screens; keep keys stable:
 * phase, desktopName, code, message, paired, pairedName, lastStatusAgeMs, notifyAccess
 * ("unknown" until the notify feature reports, then "granted" or "missing").
 */
public final class EngineState {
    /** Connection phases shown by the UI. */
    public enum Phase { DISCONNECTED, CONNECTING, CONNECTED, STALE, PAIRING, CONFIRM, READY, ERROR }

    private Phase phase = Phase.DISCONNECTED;
    private String desktopName = "", code = "", message = "", pairedName = "";
    private boolean paired;
    private long lastStatusMs;
    private String notifyAccess = "unknown";
    private String sms = "";

    public synchronized void set(Phase p, String desktopName, String code, String message) {
        this.phase = p;
        this.desktopName = desktopName == null ? "" : desktopName;
        this.code = code == null ? "" : code;
        this.message = message == null ? "" : message;
    }

    public synchronized void setPaired(boolean paired, String name) {
        this.paired = paired;
        this.pairedName = name == null ? "" : name;
    }

    /** Notification listener access, set by the notify feature: "granted" or "missing". */
    public synchronized void setNotifyAccess(boolean granted) { notifyAccess = granted ? "granted" : "missing"; }

    public synchronized void statusSent(long nowMs) { lastStatusMs = nowMs; }

    /** Permission summary of the sms feature (no content), for example "read,no-send". */
    public synchronized void setSms(String summary) { sms = summary == null ? "" : summary; }

    public synchronized Phase phase() { return phase; }

    public synchronized String toJson() {
        Map<String, Object> m = new LinkedHashMap<>();
        m.put("phase", phase.name().toLowerCase());
        m.put("desktopName", desktopName);
        m.put("code", code);
        m.put("message", message);
        m.put("paired", paired);
        m.put("pairedName", pairedName);
        m.put("sms", sms);
        m.put("lastStatusAgeMs", lastStatusMs == 0 ? -1L : System.currentTimeMillis() - lastStatusMs);
        m.put("notifyAccess", notifyAccess);
        return Json.write(m);
    }
}
