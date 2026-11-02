// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.net.LocalSocket;
import java.io.BufferedInputStream;
import java.io.Closeable;
import java.io.DataInputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import org.json.JSONObject;

/**
 * One accepted desktop connection carrying every mirror channel.
 *
 * <p>Threading: {@link #read()} is called only by the session thread. The send methods are
 * called by the session thread and the encoder thread; one lock keeps each frame contiguous.
 *
 * <p>AGENT-GUARD: a frame header and its payload must be written under the same lock hold. If
 * a video packet and a control reply interleave, the desktop decoder sees a corrupt header and
 * drops the connection.
 */
final class Connection implements Closeable {
    private final LocalSocket socket;
    private final DataInputStream in;
    private final OutputStream out;
    private final Object writeLock = new Object();
    private final byte[] header = new byte[Frames.HEADER_SIZE + Frames.PACKET_HEADER_SIZE];
    private byte[] scratch = new byte[256 * 1024];
    private volatile boolean closed;

    Connection(LocalSocket socket) throws IOException {
        this.socket = socket;
        this.in = new DataInputStream(new BufferedInputStream(socket.getInputStream(), 64 * 1024));
        this.out = socket.getOutputStream();
    }

    /** Bounds each blocking read; a timeout surfaces as an IOException from read(). */
    void setReadTimeout(int millis) throws IOException {
        socket.setSoTimeout(millis);
    }

    Frames.Frame read() throws IOException {
        return Frames.read(in);
    }

    void sendJson(int channel, int type, JSONObject body) throws IOException {
        byte[] payload = body.toString().getBytes(StandardCharsets.UTF_8);
        if (payload.length > Frames.maxPayload(channel)) {
            throw new IOException("payload too large for channel " + channel);
        }
        synchronized (writeLock) {
            Frames.writeHeader(header, channel, type, payload.length);
            out.write(header, 0, Frames.HEADER_SIZE);
            out.write(payload);
            out.flush();
        }
    }

    /** Sends one encoder output buffer; {@code data} is consumed from position to limit. */
    void sendVideoPacket(int flags, long ptsUs, ByteBuffer data) throws IOException {
        int size = data.remaining();
        int length = Frames.PACKET_HEADER_SIZE + size;
        if (length > Frames.MAX_VIDEO_PAYLOAD) {
            throw new IOException("encoded packet of " + size + " bytes exceeds the video limit");
        }
        synchronized (writeLock) {
            Frames.writeHeader(header, Frames.CH_VIDEO, Frames.V_PACKET, length);
            int p = Frames.HEADER_SIZE;
            header[p++] = (byte) flags;
            for (int shift = 56; shift >= 0; shift -= 8) {
                header[p++] = (byte) (ptsUs >>> shift);
            }
            out.write(header, 0, p);
            if (scratch.length < size) {
                scratch = new byte[Math.max(size, scratch.length * 2)];
            }
            data.get(scratch, 0, size);
            out.write(scratch, 0, size);
            out.flush();
        }
    }

    boolean isClosed() {
        return closed;
    }

    @Override
    public void close() {
        closed = true;
        try {
            socket.shutdownInput();
        } catch (IOException ignored) {
            // Already shut down by the peer.
        }
        try {
            socket.close();
        } catch (IOException ignored) {
            // Nothing left to release.
        }
    }
}
