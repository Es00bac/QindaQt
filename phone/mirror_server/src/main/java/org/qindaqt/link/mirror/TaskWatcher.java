// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import java.lang.reflect.Field;
import java.util.List;

/**
 * Notices when the mirrored app has left its virtual display (the user closed it, it crashed,
 * or it finished its last activity) so the desktop can close the window.
 *
 * <p>AGENT-NOTE: polls IActivityTaskManager.getAllRootTaskInfosOnDisplay once a second. The
 * app counts as gone only after it was seen on the display and then two polls in a row find
 * no root task with a top activity, so a slow launch or a task switch is not an exit. When the
 * method is missing the watcher stays off and the session simply streams an empty display.
 *
 * <p>Threading: its own daemon thread; the callback runs on that thread.
 */
final class TaskWatcher {
    interface Listener {
        void onAppExited(String reason);
    }

    private static final long POLL_MS = 1000;
    private static final int ABSENT_POLLS_FOR_EXIT = 2;

    private final HiddenApi api;
    private final int displayId;
    private final Listener listener;
    private volatile boolean stopped;
    private Thread thread;

    TaskWatcher(HiddenApi api, int displayId, Listener listener) {
        this.api = api;
        this.displayId = displayId;
        this.listener = listener;
    }

    void start() {
        if (api.rootTasksOnDisplay == null) {
            Ln.w("app exit detection unavailable on this Android build");
            return;
        }
        thread = new Thread(this::loop, "qindalink-mirror-tasks");
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
        boolean seen = false;
        int absent = 0;
        while (!stopped) {
            try {
                Thread.sleep(POLL_MS);
            } catch (InterruptedException e) {
                return;
            }
            int live = liveTasks();
            if (live < 0) {
                return; // the call failed; stay quiet rather than guess
            }
            if (live > 0) {
                seen = true;
                absent = 0;
            } else if (seen && ++absent >= ABSENT_POLLS_FOR_EXIT && !stopped) {
                listener.onAppExited("no-tasks");
                return;
            }
        }
    }

    /** Root tasks on the display that still have a top activity, or -1 on failure. */
    private int liveTasks() {
        try {
            List<?> tasks = (List<?>) HiddenApi.call(api.rootTasksOnDisplay, api.activityTaskManager, displayId);
            if (tasks == null) {
                return 0;
            }
            int live = 0;
            for (Object task : tasks) {
                if (topActivity(task) != null) {
                    live++;
                }
            }
            return live;
        } catch (Exception e) {
            Ln.w("task query failed: " + Ln.describe(e));
            return -1;
        }
    }

    private static Object topActivity(Object task) {
        try {
            Field field = task.getClass().getField("topActivity");
            return field.get(task);
        } catch (ReflectiveOperationException e) {
            return task; // no such field on this release: count the task itself as live
        }
    }
}
