package org.qindaqt.link.notify;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;

/**
 * One mirrorable notification as the protocol page's "Notifications" section describes it.
 * Immutable. Title and text are personal content: never log them.
 */
public final class PhoneNotification {
    /** One button. {@code reply} marks an action that takes typed text (RemoteInput). */
    public static final class Action {
        public final String id, title;
        public final boolean reply;
        public Action(String id, String title, boolean reply) { this.id = id; this.title = title; this.reply = reply; }
        @Override public boolean equals(Object o) {
            return o instanceof Action && ((Action) o).id.equals(id) && ((Action) o).title.equals(title) && ((Action) o).reply == reply;
        }
        @Override public int hashCode() { return Objects.hash(id, title, reply); }
    }

    public final String key, pkg, app, title, text, iconHash;
    public final long when;
    public final List<Action> actions;

    public PhoneNotification(String key, String pkg, String app, String title, String text, long when,
                             String iconHash, List<Action> actions) {
        this.key = key; this.pkg = pkg; this.app = app; this.title = title; this.text = text; this.when = when;
        this.iconHash = iconHash == null ? "" : iconHash;
        this.actions = Collections.unmodifiableList(new ArrayList<>(actions));
    }
}
