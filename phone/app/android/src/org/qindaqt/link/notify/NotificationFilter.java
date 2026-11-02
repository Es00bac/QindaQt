package org.qindaqt.link.notify;

/**
 * Which phone notifications are mirrored. Pure so the rule is unit-tested on the JVM; the
 * Android source feeds it the flags of each StatusBarNotification.
 */
public final class NotificationFilter {
    private NotificationFilter() { }

    /**
     * @param ongoing         FLAG_ONGOING_EVENT (media players, calls in progress, ...)
     * @param foregroundService FLAG_FOREGROUND_SERVICE
     * @param groupSummary    FLAG_GROUP_SUMMARY (the collapsed header of a bundle)
     * @param pkg             posting package
     * @param ownPkg          QindaLink's package
     */
    public static boolean mirror(boolean ongoing, boolean foregroundService, boolean groupSummary, String pkg, String ownPkg) {
        return !ongoing && !foregroundService && !groupSummary && pkg != null && !pkg.isEmpty() && !pkg.equals(ownPkg);
    }
}
