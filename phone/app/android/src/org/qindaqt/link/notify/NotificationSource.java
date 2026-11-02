package org.qindaqt.link.notify;

import java.util.List;

/**
 * What the notify feature needs from the system. Android implements it with a
 * NotificationListenerService (android/NotificationAndroidSource); tests use a fake.
 * Everything handed out is already filtered by {@link NotificationFilter}.
 */
public interface NotificationSource {
    /** Callbacks may arrive on any thread. */
    interface Listener {
        void onPosted(PhoneNotification n);
        void onRemoved(String key);
        /** The listener service was granted (true) or lost (false). On true the feature re-sends a snapshot. */
        void onAccessChanged(boolean granted);
    }

    /** Null unregisters. */
    void setListener(Listener l);

    /** True while the listener service is bound, i.e. notification access is granted and live. */
    boolean accessGranted();

    /** Current mirrored notifications; empty without access. */
    List<PhoneNotification> active();

    /** cancelNotification(key). False when access is missing. */
    boolean dismiss(String key);

    /**
     * Fires action {@code actionId}; {@code replyText} is non-null for a reply action. Returns null on
     * success or a protocol error code: "target-changed" (key gone), "bad-request" (unknown action, or
     * text for a non-reply action), "denied" (no access), "internal" (Android refused).
     */
    String perform(String key, String actionId, String replyText);

    /** PNG bytes for an icon hash this source produced, or null. */
    byte[] icon(String hash);
}
