// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.os.Looper;
import java.io.IOException;

/**
 * app_process entry point of the QindaLink mirror server.
 *
 * <pre>
 * echo &lt;secret&gt; | CLASSPATH=&lt;jar or apk&gt; app_process / org.qindaqt.link.mirror.Server \
 *     version=1 socket=&lt;suffix&gt; [verbose=1] [accept-timeout-ms=N]
 * CLASSPATH=... app_process / org.qindaqt.link.mirror.Server probe
 * </pre>
 *
 * <p>AGENT-CONTRACT: the desktop (src/apps/link_mirror/src/adb_session.cpp) builds this
 * command line, writes the secret as the first stdin line and forwards
 * {@code localabstract:qindalink-mirror-<suffix>}. Exit codes: 0 after a session, 2 bad
 * arguments, stdin or version, 3 incomplete hidden API in probe mode, 4 no greeted session
 * before the accept deadline, 5 too many failed greetings.
 *
 * <p>AGENT-GUARD: this class must not depend on the QindaLink phone app's code. It ships in the
 * APK but runs as the shell user in its own process, and it must never outlive its desktop
 * (ListenPolicy).
 */
public final class Server {
    private static volatile boolean greeted;

    private Server() {}

    @SuppressWarnings("deprecation") // Looper.prepareMainLooper: app_process has no other main looper
    public static void main(String[] args) {
        if (args.length == 1 && "probe".equals(args[0])) {
            HiddenApi api = HiddenApi.probe();
            System.out.println(api.report());
            System.exit(api.complete() ? 0 : 3);
        }
        String version = null;
        String suffix = null;
        int acceptTimeout = ListenPolicy.DEFAULT_ACCEPT_TIMEOUT_MS;
        for (String arg : args) {
            int eq = arg.indexOf('=');
            String key = eq < 0 ? arg : arg.substring(0, eq);
            String value = eq < 0 ? "" : arg.substring(eq + 1);
            switch (key) {
                case "version":
                    version = value;
                    break;
                case "socket":
                    suffix = value;
                    break;
                case "verbose":
                    Ln.setVerbose("1".equals(value));
                    break;
                case "accept-timeout-ms":
                    acceptTimeout = parseInt(value, ListenPolicy.DEFAULT_ACCEPT_TIMEOUT_MS);
                    break;
                default:
                    Ln.w("ignoring unknown argument " + key);
            }
        }
        if (!String.valueOf(Frames.PROTOCOL_VERSION).equals(version)) {
            fail(2, "desktop asked for protocol version " + version + ", server speaks "
                    + Frames.PROTOCOL_VERSION);
        }
        if (!SessionSecret.validSuffix(suffix)) {
            fail(2, "socket must be 8-32 lowercase hex characters");
        }
        // The deadline covers a stdin that never delivers the secret as well as a desktop
        // that never connects.
        startDeadline(acceptTimeout);
        String secret = null;
        try {
            secret = SessionSecret.read(System.in);
        } catch (IOException e) {
            fail(2, e.getMessage());
        }
        Looper.prepareMainLooper();
        HiddenApi api = HiddenApi.probe();
        Ln.i(api.report());
        System.exit(serve(api, suffix, secret));
    }

    private static int serve(HiddenApi api, String suffix, String secret) {
        LocalServerSocket server;
        try {
            server = new LocalServerSocket(Frames.SOCKET_PREFIX + suffix);
        } catch (IOException e) {
            Ln.e("cannot listen on the mirror socket", e);
            return 2;
        }
        Ln.i("listening");
        ListenPolicy policy = new ListenPolicy();
        try {
            while (true) {
                LocalSocket client = server.accept();
                Session session = new Session(new Connection(client), api, secret, () -> greeted = true);
                if (session.run()) {
                    return 0;
                }
                if (policy.recordFailedGreeting()) {
                    Ln.w("giving up after " + policy.failedGreetings() + " failed greetings");
                    return 5;
                }
            }
        } catch (IOException e) {
            Ln.e("accept failed", e);
            return 1;
        } finally {
            try {
                server.close();
            } catch (IOException ignored) {
                // Already closed.
            }
        }
    }

    /** Exits the process at the deadline unless a desktop has completed hello. */
    private static void startDeadline(int timeoutMs) {
        Thread watchdog = new Thread(() -> {
            try {
                Thread.sleep(timeoutMs);
            } catch (InterruptedException e) {
                return;
            }
            if (!greeted) {
                Ln.w("no desktop completed hello within " + timeoutMs + " ms; exiting");
                System.exit(4);
            }
        }, "qindalink-mirror-accept-deadline");
        watchdog.setDaemon(true);
        watchdog.start();
    }

    private static int parseInt(String value, int fallback) {
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return fallback;
        }
    }

    private static void fail(int code, String message) {
        Ln.w(message);
        System.exit(code);
    }
}
