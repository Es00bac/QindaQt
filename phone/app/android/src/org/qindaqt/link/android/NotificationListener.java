package org.qindaqt.link.android;

import android.service.notification.NotificationListenerService;
import android.service.notification.StatusBarNotification;

/**
 * Bound by the system once the user (or `cmd notification allow_listener`) grants notification
 * access. It only forwards events to {@link NotificationAndroidSource}; all policy lives there
 * and in the pure-Java notify package.
 *
 * AGENT-GUARD: never log anything taken from a StatusBarNotification except package-free counts.
 */
public final class NotificationListener extends NotificationListenerService {
    @Override public void onListenerConnected() { NotificationAndroidSource.INSTANCE.connected(this); }
    @Override public void onListenerDisconnected() { NotificationAndroidSource.INSTANCE.disconnected(this); }
    @Override public void onNotificationPosted(StatusBarNotification sbn) { NotificationAndroidSource.INSTANCE.posted(sbn); }
    @Override public void onNotificationRemoved(StatusBarNotification sbn) { NotificationAndroidSource.INSTANCE.removed(sbn); }
}
