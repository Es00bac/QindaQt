// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

/**
 * Time and retry limits that keep a shell-uid server from outliving its desktop.
 *
 * <p>AGENT-GUARD: the server runs as the shell user, which is more privileged than any app.
 * It must never be left running without its desktop: an unauthenticated peer gets
 * HELLO_TIMEOUT_MS, the process exits after MAX_FAILED_GREETINGS bad peers or at the accept
 * deadline without a greeted session, and a greeted session ends after IDLE_TIMEOUT_MS with
 * no frame (the desktop pings every 5 s).
 *
 * <p>Pure Java (no android.*) so phone/mirror_server/tests can run it on the host JVM.
 */
final class ListenPolicy {
    static final int HELLO_TIMEOUT_MS = 5_000;
    static final int IDLE_TIMEOUT_MS = 30_000;
    static final int MAX_FAILED_GREETINGS = 3;
    static final int DEFAULT_ACCEPT_TIMEOUT_MS = 20_000;

    private int failedGreetings;

    /** Records a dropped peer; returns true when the server must give up and exit. */
    boolean recordFailedGreeting() {
        failedGreetings++;
        return failedGreetings >= MAX_FAILED_GREETINGS;
    }

    int failedGreetings() {
        return failedGreetings;
    }
}
