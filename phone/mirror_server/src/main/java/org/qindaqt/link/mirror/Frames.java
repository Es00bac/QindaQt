// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import java.io.DataInputStream;
import java.io.EOFException;
import java.io.IOException;

/**
 * Wire constants and frame reading for QindaLink mirror protocol 1.
 *
 * <p>AGENT-CONTRACT: docs/wiki/reference/qindalink-mirror-v1.md is normative. The desktop
 * codec is src/apps/link_mirror/src/mirror_protocol.cpp; a change to any value here changes
 * that file, the reference page and the framing test in one commit.
 */
final class Frames {
    static final int PROTOCOL_VERSION = 1;
    static final String SOCKET_PREFIX = "qindalink-mirror-";
    static final int HEADER_SIZE = 8;

    static final int CH_CONTROL = 0x01;
    static final int CH_VIDEO = 0x02;
    static final int CH_INVENTORY = 0x03;

    static final int MAX_CONTROL_PAYLOAD = 1 << 20;
    static final int MAX_VIDEO_PAYLOAD = 16 << 20;

    // Control, desktop -> server.
    static final int C_HELLO = 0x01;
    static final int C_START = 0x02;
    static final int C_RESIZE = 0x03;
    static final int C_TOUCH = 0x04;
    static final int C_MOUSE = 0x05;
    static final int C_SCROLL = 0x06;
    static final int C_KEY = 0x07;
    static final int C_TEXT = 0x08;
    static final int C_SYNC = 0x09;
    static final int C_STOP = 0x0A;
    static final int C_PING = 0x0B;
    // Control, server -> desktop.
    static final int C_READY = 0x81;
    static final int C_DISPLAY = 0x82;
    static final int C_ERROR = 0x83;
    static final int C_APP_EXITED = 0x84;
    static final int C_SCREEN_STATE = 0x85;

    // Video, server -> desktop.
    static final int V_FORMAT = 0x01;
    static final int V_PACKET = 0x02;
    static final int PACKET_FLAG_CONFIG = 0x01;
    static final int PACKET_FLAG_KEY = 0x02;
    static final int PACKET_HEADER_SIZE = 9;

    // Inventory.
    static final int I_LIST = 0x01;
    static final int I_APP = 0x81;
    static final int I_END = 0x82;

    private Frames() {}

    static final class Frame {
        final int channel;
        final int type;
        final byte[] payload;

        Frame(int channel, int type, byte[] payload) {
            this.channel = channel;
            this.type = type;
            this.payload = payload;
        }
    }

    /** Thrown for a header that breaks the framing rules; the connection must close. */
    static final class ProtocolException extends IOException {
        private static final long serialVersionUID = 1L;

        ProtocolException(String message) {
            super(message);
        }
    }

    static int maxPayload(int channel) {
        switch (channel) {
            case CH_CONTROL:
            case CH_INVENTORY:
                return MAX_CONTROL_PAYLOAD;
            case CH_VIDEO:
                return MAX_VIDEO_PAYLOAD;
            default:
                return -1;
        }
    }

    static void writeHeader(byte[] out, int channel, int type, int length) {
        out[0] = (byte) channel;
        out[1] = (byte) type;
        out[2] = 0;
        out[3] = 0;
        out[4] = (byte) (length >>> 24);
        out[5] = (byte) (length >>> 16);
        out[6] = (byte) (length >>> 8);
        out[7] = (byte) length;
    }

    /**
     * Reads one frame. Returns null on a clean end of stream before a header starts.
     *
     * <p>AGENT-GUARD: validation order (channel, reserved, length bounds) matches the desktop
     * FrameDecoder so both sides reject the same bytes; see the framing test vectors.
     */
    static Frame read(DataInputStream in) throws IOException {
        byte[] header = new byte[HEADER_SIZE];
        int first = in.read(header, 0, HEADER_SIZE);
        if (first < 0) {
            return null;
        }
        if (first < HEADER_SIZE) {
            in.readFully(header, first, HEADER_SIZE - first);
        }
        int channel = header[0] & 0xff;
        int type = header[1] & 0xff;
        int max = maxPayload(channel);
        if (max < 0) {
            throw new ProtocolException("unknown channel " + channel);
        }
        if (header[2] != 0 || header[3] != 0) {
            throw new ProtocolException("reserved header bytes are not zero");
        }
        long length = ((header[4] & 0xffL) << 24) | ((header[5] & 0xffL) << 16)
                | ((header[6] & 0xffL) << 8) | (header[7] & 0xffL);
        if (length < 1 || length > max) {
            throw new ProtocolException("payload length " + length + " outside 1.." + max);
        }
        byte[] payload = new byte[(int) length];
        try {
            in.readFully(payload);
        } catch (EOFException e) {
            throw new ProtocolException("stream ended inside a frame");
        }
        return new Frame(channel, type, payload);
    }
}
