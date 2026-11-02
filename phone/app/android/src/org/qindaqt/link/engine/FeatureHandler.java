package org.qindaqt.link.engine;

import java.io.IOException;
import java.util.List;
import org.qindaqt.link.proto.Envelope;

/**
 * A phone-side feature (status, notify, sms, input, macro, vault, apps) that owns
 * one capability string and the frame namespaces that go with it.
 *
 * AGENT-CONTRACT: mirrors QindaQt::Link::FeatureHandler in
 * src/services/link_transport/include/qindaqt/services/link_transport/feature.h
 * (desktop side). Rules shared by both sides: a handler is called only after the
 * session is READY; every call is made on the session thread (no locking needed
 * for handler state touched only from these methods); frames are routed by the
 * namespace, the text before the first '.' of the type (or the whole type); a side
 * sends a namespace's frames only if the peer listed the capability in hello.caps,
 * which {@link FeatureContext#send} enforces. Handlers must never log vault.* bodies.
 * Handler instances are shared across sessions: reset state in {@link #onReady}.
 */
public interface FeatureHandler {
    /** Capability string advertised in hello.caps (for example "status"). */
    String capability();

    /** Frame namespaces owned by this handler; every one must be allowed by the peer's caps entry. */
    List<String> namespaces();

    /** Session entered READY. The context is valid until {@link #onClosed}. */
    void onReady(FeatureContext ctx) throws IOException;

    /** Called about every 500 ms and after {@link FeatureContext#wake()} while READY. */
    default void onTick(long nowMs) throws IOException { }

    /** Returns true if the frame was handled; false makes the session answer unknown-type to a request. */
    boolean onFrame(Envelope e) throws IOException;

    /** The session ended (any reason). Drop the context. */
    void onClosed();
}
