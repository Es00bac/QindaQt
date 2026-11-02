package org.qindaqt.link.proto;

import java.io.ByteArrayOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.CharacterCodingException;
import java.nio.charset.CodingErrorAction;
import java.nio.charset.StandardCharsets;
import java.util.Map;

/**
 * Length-prefixed frame encoder/decoder for QindaLink protocol 1.
 *
 * AGENT-CONTRACT: mirrors the C++ codec in src/services/link_*; both are
 * checked by tests/link/vectors/frames-v1.json. Pure Java, no android.*.
 * Not thread-safe: one decoder per connection, driven from the reader thread.
 */
public final class FrameCodec {
    public static final int MAX_PAYLOAD = 4194304;

    /** Decoder outcomes named by the protocol page. */
    public enum Status { FRAME, NEEDS_MORE, BAD_LENGTH, BAD_PAYLOAD, BAD_ENVELOPE }

    public static final class Result {
        public final Status status;
        public final Envelope envelope;
        Result(Status s, Envelope e) { status = s; envelope = e; }
    }

    private byte[] buf = new byte[256];
    private int len;

    /** Appends received bytes. */
    public void feed(byte[] data, int off, int n) {
        if (len + n > buf.length) {
            byte[] nb = new byte[Math.max(buf.length * 2, len + n)];
            System.arraycopy(buf, 0, nb, 0, len);
            buf = nb;
        }
        System.arraycopy(data, off, buf, len, n);
        len += n;
    }

    /** Extracts the next frame. After any BAD_* result the connection must close. */
    public Result next() {
        if (len < 4) return new Result(Status.NEEDS_MORE, null);
        long n = ((buf[0] & 0xffL) << 24) | ((buf[1] & 0xff) << 16) | ((buf[2] & 0xff) << 8) | (buf[3] & 0xff);
        if (n < 1 || n > MAX_PAYLOAD) return new Result(Status.BAD_LENGTH, null);
        if (len < 4 + n) return new Result(Status.NEEDS_MORE, null);
        int pl = (int) n;
        byte[] payload = new byte[pl];
        System.arraycopy(buf, 4, payload, 0, pl);
        System.arraycopy(buf, 4 + pl, buf, 0, len - 4 - pl);
        len -= 4 + pl;
        return parsePayload(payload);
    }

    @SuppressWarnings("unchecked")
    static Result parsePayload(byte[] payload) {
        String text;
        try {
            text = StandardCharsets.UTF_8.newDecoder()
                    .onMalformedInput(CodingErrorAction.REPORT)
                    .onUnmappableCharacter(CodingErrorAction.REPORT)
                    .decode(ByteBuffer.wrap(payload)).toString();
        } catch (CharacterCodingException e) {
            return new Result(Status.BAD_PAYLOAD, null);
        }
        if (text.length() > 0 && text.charAt(0) == '﻿') return new Result(Status.BAD_PAYLOAD, null);
        Object v;
        try {
            v = Json.parse(text);
        } catch (Json.ParseException e) {
            return new Result(Status.BAD_PAYLOAD, null);
        }
        if (!(v instanceof Map)) return new Result(Status.BAD_PAYLOAD, null);
        Envelope env = Envelope.fromObject((Map<String, Object>) v);
        if (env == null) return new Result(Status.BAD_ENVELOPE, null);
        return new Result(Status.FRAME, env);
    }

    /** Encodes one frame: 4-byte big-endian length plus compact JSON. */
    public static byte[] encode(Envelope e) {
        return encodeObject(e.toObject());
    }

    public static byte[] encodeObject(Map<String, Object> o) {
        byte[] p = Json.write(o).getBytes(StandardCharsets.UTF_8);
        if (p.length > MAX_PAYLOAD) throw new IllegalArgumentException("frame too large");
        ByteArrayOutputStream out = new ByteArrayOutputStream(p.length + 4);
        out.write(p.length >>> 24);
        out.write(p.length >>> 16);
        out.write(p.length >>> 8);
        out.write(p.length);
        out.write(p, 0, p.length);
        return out.toByteArray();
    }
}
