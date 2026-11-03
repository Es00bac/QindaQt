// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

/**
 * Reports the phone's own screen turning off and on while the mirror display shares its power
 * group, so the desktop can show a paused state instead of a frozen picture.
 *
 * <p>AGENT-NOTE: user decision 2026-11-02, "pause on screen-off": the server never holds the
 * phone's main screen awake and never turns the panel off. On builds where the mirror display
 * lands in display group 0 (SM-S948U, SDK 37) the mirror simply sleeps with the phone, and this
 * watcher tells the desktop. Polling (500 ms) is used because app_process has no Context to
 * register ACTION_SCREEN_OFF/ON receivers on.
 *
 * <p>Threading: its own daemon thread; the listener runs on that thread.
 */
final class ScreenStateWatcher {
    interface Listener {
        void onScreenState(String state);
    }

    private static final long POLL_MS = 500;

    private final HiddenApi api;
    private final Listener listener;
    private volatile boolean stopped;
    private Thread thread;

    ScreenStateWatcher(HiddenApi api, Listener listener) {
        this.api = api;
        this.listener = listener;
    }

    void start() {
        if (api.isInteractive == null && api.isDisplayInteractive == null) {
            Ln.w("phone screen state unavailable on this Android build");
            return;
        }
        Ln.i("watching the phone screen (mirror pauses while it is off)");
        thread = new Thread(this::loop, "qindalink-mirror-screen");
        thread.setDaemon(true);
        thread.start();
    }

    void stop() {
        stopped = true;
        if (thread != null) {
            thread.interrupt();
        }
    }

    private void loop() {
        ScreenStateTracker tracker = new ScreenStateTracker(true);
        while (!stopped) {
            try {
                Thread.sleep(POLL_MS);
            } catch (InterruptedException e) {
                return;
            }
            Boolean interactive = phoneInteractive();
            if (interactive == null) {
                return; // the query failed; stay quiet rather than guess
            }
            String change = tracker.update(interactive);
            if (change != null && !stopped) {
                Ln.i("phone screen " + change);
                listener.onScreenState(change);
            }
        }
    }

    private Boolean phoneInteractive() {
        try {
            Object value = api.isInteractive != null
                    ? HiddenApi.call(api.isInteractive, api.powerManager)
                    : HiddenApi.call(api.isDisplayInteractive, api.powerManager, 0);
            return Boolean.TRUE.equals(value);
        } catch (Exception e) {
            Ln.w("screen state query failed: " + Ln.describe(e));
            return null;
        }
    }
}
