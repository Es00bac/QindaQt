// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

/**
 * Turns polled phone-screen interactivity into on/off transitions for screen-state messages.
 *
 * <p>Pure Java (no android.*) so phone/mirror_server/tests can run it on the host JVM.
 */
final class ScreenStateTracker {
    static final String ON = "on";
    static final String OFF = "off";

    private boolean on;

    ScreenStateTracker(boolean initiallyOn) {
        on = initiallyOn;
    }

    /** Returns ON or OFF when the state changed since the last call, otherwise null. */
    String update(boolean interactive) {
        if (interactive == on) {
            return null;
        }
        on = interactive;
        return on ? ON : OFF;
    }
}
