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

    private static void cmd(Session.Command c) {
        LinkEngine e = LinkService.engine();
        if (e != null) e.command(c);
    }
}
