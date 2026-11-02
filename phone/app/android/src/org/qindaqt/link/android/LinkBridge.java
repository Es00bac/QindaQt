package org.qindaqt.link.android;

import android.content.Context;
import org.qindaqt.link.engine.LinkEngine;
import org.qindaqt.link.engine.Session;

/**
 * The single class the Qt/JNI layer calls (phone/app/LinkBridge.cpp). All
 * methods are static, non-blocking and safe when the engine is not running.
 *
 * AGENT-CONTRACT: the method names and signatures here are looked up by name
 * from C++ via QJniObject::callStaticMethod; change both sides together.
 */
public final class LinkBridge {
    private LinkBridge() {}

    public static void startService(Context ctx) { LinkService.start(ctx); }

    /** JSON from EngineState.toJson(), or a stub when the service is down. */
    public static String stateJson() {
        LinkEngine e = LinkService.engine();
        return e == null ? "{\"phase\":\"stopped\",\"message\":\"Service not running\"}" : e.state().toJson();
    }

    public static void pair() { cmd(Session.Command.PAIR); }
    public static void pairAgain() { cmd(Session.Command.PAIR_AGAIN); }
    public static void accept() { cmd(Session.Command.ACCEPT); }
    public static void reject() { cmd(Session.Command.REJECT); }

    public static void forget() {
        LinkEngine e = LinkService.engine();
        if (e != null) e.forget();
    }

    /** UI facts (battery, network, permission states) from {@link DeviceFacts}; never blocks. */
    public static String deviceJson(Context ctx) { return DeviceFacts.json(ctx); }

    public static void openSettings(Context ctx, String key) { DeviceFacts.openSettings(ctx, key); }

    // ---- input and macros (org.qindaqt.link.input / .macro) -----------------------------------------
    // AGENT-CONTRACT: all non-blocking; the engine sends from its session thread.

    public static String inputJson() { return LinkService.inputFeature.json(); }
    public static void inputBegin() { LinkService.inputFeature.begin(); }
    public static void inputEnd() { LinkService.inputFeature.end(); }

    /** One touch frame from the touchpad; true when the UI should give haptic feedback. */
    public static boolean touch(int fingers, double x0, double y0, double x1, double y1) {
        return LinkService.inputFeature.touch(fingers, x0, y0, x1, y1);
    }

    public static void setSensitivity(double s) { LinkService.inputFeature.setSensitivity(s); }
    public static void click(String button) { LinkService.inputFeature.click(button); }
    public static void sendKey(String name, int mods) { LinkService.inputFeature.sendKey(name, mods); }
    public static void sendChar(int codePoint, int mods) { LinkService.inputFeature.sendChar(codePoint, mods); }
    public static void sendText(String text) { LinkService.inputFeature.sendText(text); }

    public static String macroJson() { return LinkService.macroFeature.json(); }
    public static void macroRun(String id) { LinkService.macroFeature.run(id); }
    public static void macroCancel() { LinkService.macroFeature.cancel(); }

    /** A short tick on the activity's window; needs no permission. */
    public static void haptic(Context ctx) {
        if (!(ctx instanceof android.app.Activity)) return;
        final android.app.Activity a = (android.app.Activity) ctx;
        a.runOnUiThread(() -> a.getWindow().getDecorView()
                .performHapticFeedback(android.view.HapticFeedbackConstants.CONTEXT_CLICK));
    }

    private static void cmd(Session.Command c) {
        LinkEngine e = LinkService.engine();
        if (e != null) e.command(c);
    }
}
