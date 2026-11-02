package org.qindaqt.link.engine;

import java.io.IOException;
import org.qindaqt.link.proto.Envelope;

/**
 * What a {@link FeatureHandler} may do with its session. Calls are valid only on
 * the session thread, except {@link #wake()} which is thread-safe.
 */
public interface FeatureContext {
    /**
     * Sends a feature frame through the bounded writer. Returns false, sending nothing, when the frame's
     * namespace is not owned by the calling handler or the peer did not advertise the handler's capability.
     * Throws when the transport is failing (the session is ending).
     */
    boolean send(Envelope e) throws IOException;

    /** A fresh request id (1, 2, ...), unique within the session. */
    long nextId();

    /** True if the desktop listed this capability in its hello. */
    boolean peerHas(String capability);

    /** Answers request {@code id} with {@code ok}. Core frame: not capability-gated. */
    void replyOk(long id) throws IOException;

    /** Answers request {@code id} with error{code,message}. Core frame: not capability-gated. */
    void replyError(long id, String code, String message) throws IOException;

    /** Asks the session to call {@link FeatureHandler#onTick} soon. Thread-safe. */
    void wake();
}
