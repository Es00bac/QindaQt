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

    /** Receives the reply to a {@link #request}: an {@code ok}, an {@code error} or a typed frame whose {@code re} matches. */
    interface ReplyHandler {
        void onReply(Envelope reply) throws IOException;
    }

    /**
     * Sends a request frame with a fresh id and routes the one reply carrying {@code re == id} to
     * {@code handler} (on the session thread), whatever its type. Same refusal rules as {@link #send}:
     * returns false and sends nothing when the namespace or capability check fails. A reply that never
     * arrives is the feature's business (use {@link FeatureHandler#onTick}); the handler is dropped when
     * the session ends. The default implementation does not support requests.
     */
    default boolean request(String type, java.util.Map<String, Object> body, ReplyHandler handler) throws IOException {
        throw new UnsupportedOperationException("requests are not supported by this context");
    }

    /** True if the desktop listed this capability in its hello. */
    boolean peerHas(String capability);

    /** Answers request {@code id} with {@code ok}. Core frame: not capability-gated. */
    void replyOk(long id) throws IOException;

    /** Answers request {@code id} with error{code,message}. Core frame: not capability-gated. */
    void replyError(long id, String code, String message) throws IOException;

    /** Asks the session to call {@link FeatureHandler#onTick} soon. Thread-safe. */
    void wake();
}
