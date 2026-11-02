package org.qindaqt.link.engine;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * An established, handshaken transport to a desktop. Implemented over TLS on
 * Android and over plain sockets in tests.
 *
 * AGENT-CONTRACT: {@link #peerSpki()} and {@link #ownSpki()} are the raw
 * 32-byte SHA-256 digests of the certificates as seen in THIS TLS session
 * (never values claimed in a frame). Pairing math depends on them.
 */
public interface Link {
    InputStream in() throws IOException;
    OutputStream out() throws IOException;
    byte[] peerSpki();
    byte[] ownSpki();
    /** Idempotent; unblocks a reader stuck in {@code in().read}. */
    void close();
}
