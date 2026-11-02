package org.qindaqt.link.engine;

import java.util.Map;

/** Supplies the body of a {@code status} frame (battery and network only). */
public interface StatusSource {
    /** Body per the protocol page's status table. Must not block for long. */
    Map<String, Object> snapshot();
}
