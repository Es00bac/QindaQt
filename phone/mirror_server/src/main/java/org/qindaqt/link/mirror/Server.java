// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.net.LocalServerSocket;
import android.net.LocalSocket;
import android.os.Looper;
import java.io.IOException;
import java.util.regex.Pattern;

/**
 * app_process entry point of the QindaLink mirror server.
 *
 * <pre>
 * CLASSPATH=&lt;jar or apk&gt; app_process / org.qindaqt.link.mirror.Server version=1 token=&lt;hex&gt;
 *     [verbose=1] [accept-timeout-ms=N]
 * CLASSPATH=... app_process / org.qindaqt.link.mirror.Server probe
 * </pre>
 *
 * <p>AGENT-CONTRACT: the desktop (src/apps/link_mirror/src/adb_session.cpp) builds this
 * command line and forwards {@code localabstract:qindalink-mirror-<token>}. Exit codes: 0 after
 * a session, 2 bad arguments or version, 3 incomplete hidden API in probe mode, 4 no client
 * before the accept deadline.
 *
 * <p>AGENT-GUARD: this class must not depend on the QindaLink phone app's code. It ships in the
 * APK but runs as the shell user in its own process.
 */
public final class Server {
    private static final Pattern TOKEN = Pattern.compile("[0-9a-f]{16,64}");
    private static final int DEFAULT_ACCEPT_TIMEOUT_MS = 20_000;

    private Server() {}

    @SuppressWarnings("deprecation") // Looper.prepareMainLooper: app_process has no other main looper
    public static void main(String[] args) {
        if (args.length == 1 && "probe".equals(args[0])) {
            HiddenApi api = HiddenApi.probe();
            System.out.println(api.report());
            System.exit(api.complete() ? 0 : 3);
        }
        String version = null;
        String token = null;
        int acceptTimeout = DEFAULT_ACCEPT_TIMEOUT_MS;
        for (String arg : args) {
            int eq = arg.indexOf('=');
            String key = eq < 0 ? arg : arg.substring(0, eq);
            String value = eq < 0 ? "" : arg.substring(eq + 1);
            switch (key) {
                case "version":
                    version = value;
                    break;
                case "token":
                    token = value;
                    break;
                case "verbose":
                    Ln.setVerbose("1".equals(value));
                    break;
                case "accept-timeout-ms":
                    acceptTimeout = parseInt(value, DEFAULT_ACCEPT_TIMEOUT_MS);
                    break;
                default:
                    Ln.w("ignoring unknown argument " + key);
            }
        }
        if (!String.valueOf(Frames.PROTOCOL_VERSION).equals(version)) {
            fail(2, "desktop asked for protocol version " + version + ", server speaks "
                    + Frames.PROTOCOL_VERSION);
        }
        if (token == null || !TOKEN.matcher(token).matches()) {
            fail(2, "token must be 16-64 lowercase hex characters");
        }
        Looper.prepareMainLooper();
        HiddenApi api = HiddenApi.probe();
        Ln.i(api.report());
        System.exit(serve(api, token, acceptTimeout));
    }

    private static int serve(HiddenApi api, String token, int acceptTimeoutMs) {
        LocalServerSocket server;
        try {
            server = new LocalServerSocket(Frames.SOCKET_PREFIX + token);
        } catch (IOException e) {
            Ln.e("cannot listen on the mirror socket", e);
            return 2;
        }
        Thread watchdog = startWatchdog(server, acceptTimeoutMs);
        Ln.i("listening");
        try {
            while (true) {
                LocalSocket client;
                try {
                    client = server.accept();
                } catch (IOException e) {
                    Ln.w("no desktop connected before the accept deadline");
                    return 4;
                }
                Session session = new Session(new Connection(client), api, token);
                if (session.run()) {
                    watchdog.interrupt();
                    return 0;
                }
            }
        } catch (IOException e) {
            Ln.e("connection setup failed", e);
            return 1;
        } finally {
            closeQuietly(server);
        }
    }

    /** Closes the listener after the deadline unless a session has started and interrupted it. */
    private static Thread startWatchdog(LocalServerSocket server, int timeoutMs) {
        Thread watchdog = new Thread(() -> {
            try {
                Thread.sleep(timeoutMs);
                closeQuietly(server);
            } catch (InterruptedException ignored) {
                // A session ended first.
            }
        }, "qindalink-mirror-accept-deadline");
        watchdog.setDaemon(true);
        watchdog.start();
        return watchdog;
    }

    private static void closeQuietly(LocalServerSocket server) {
        try {
            server.close();
        } catch (IOException ignored) {
            // Already closed.
        }
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
