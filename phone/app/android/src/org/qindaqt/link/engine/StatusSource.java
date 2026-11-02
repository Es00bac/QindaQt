package org.qindaqt.link.engine;

import java.util.Map;

/** Supplies the body of a {@code status} frame (battery and network only). */
public interface StatusSource {
    /** Body per the protocol page's status table. Must not block for long. */
    Map<String, Object> snapshot();

    /**
     * Registers a callback invoked (on any thread) when battery or network
     * changed; null unregisters. Lets the session sleep between changes
     * instead of querying the system every tick. Default: no notifications.
     */
    default void setListener(Runnable r) { }
}
