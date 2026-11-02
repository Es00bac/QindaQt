// SPDX-License-Identifier: GPL-3.0-or-later
package org.qindaqt.link.mirror;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Bundle;
import android.view.Surface;
import java.io.IOException;
import java.nio.ByteBuffer;

/**
 * MediaCodec surface encoder for one mirror stream generation.
 *
 * <p>Each {@link #start} begins a new generation: the sink receives one format callback before
 * any packet. A resize stops the old generation and starts a new one with a fresh codec, so
 * the desktop always sees new parameter sets before the first frame of a new size.
 *
 * <p>Threading: start/stop/requestSyncFrame run on the session thread; output is drained on a
 * private thread that calls the sink.
 */
final class ScreenEncoder {
    interface Sink {
        void onFormat(String codec, int width, int height) throws IOException;

        void onPacket(int flags, long ptsUs, ByteBuffer data) throws IOException;

        void onEncoderFailure(Exception error);
    }

    /** Re-encode the previous frame after this long so a static screen still streams. */
    private static final long REPEAT_FRAME_US = 100_000;
    private static final int I_FRAME_INTERVAL_SEC = 10;

    private final String codecName;
    private final String mime;
    private final int bitrate;
    private final int maxFps;
    private final Sink sink;
    private MediaCodec codec;
    private Surface surface;
    private Thread drain;
    private volatile boolean stopping;

    ScreenEncoder(String codecName, int bitrate, int maxFps, Sink sink) {
        this.codecName = codecName;
        this.mime = "hevc".equals(codecName) ? MediaFormat.MIMETYPE_VIDEO_HEVC : MediaFormat.MIMETYPE_VIDEO_AVC;
        this.bitrate = bitrate;
        this.maxFps = maxFps;
        this.sink = sink;
    }

    /** Returns {width, height} that the chosen encoder accepts, close to the request. */
    int[] fitSize(int width, int height) {
        MediaCodecInfo info = chooseEncoder();
        MediaCodecInfo.VideoCapabilities caps = info.getCapabilitiesForType(mime).getVideoCapabilities();
        int alignW = Math.max(8, caps.getWidthAlignment());
        int alignH = Math.max(8, caps.getHeightAlignment());
        double scale = 1.0;
        for (int attempt = 0; attempt < 40; attempt++) {
            int w = align((int) (width * scale), alignW);
            int h = align((int) (height * scale), alignH);
            if (caps.isSizeSupported(w, h)) {
                return new int[] {w, h};
            }
            scale *= 0.92;
        }
        throw new IllegalArgumentException("encoder " + info.getName() + " rejects " + width + "x" + height);
    }

    Surface start(int width, int height) throws IOException {
        MediaCodecInfo info = chooseEncoder();
        codec = MediaCodec.createByCodecName(info.getName());
        configure(info, width, height);
        surface = codec.createInputSurface();
        sink.onFormat(codecName, width, height);
        codec.start();
        stopping = false;
        drain = new Thread(this::drainLoop, "qindalink-mirror-encoder");
        drain.start();
        Ln.i("encoder " + info.getName() + " " + codecName + " " + width + "x" + height
                + " " + bitrate + " bps");
        return surface;
    }

    void requestSyncFrame() {
        MediaCodec current = codec;
        if (current == null) {
            return;
        }
        Bundle params = new Bundle();
        params.putInt(MediaCodec.PARAMETER_KEY_REQUEST_SYNC_FRAME, 0);
        try {
            current.setParameters(params);
        } catch (IllegalStateException e) {
            Ln.w("sync frame request ignored: " + Ln.describe(e));
        }
    }

    void stop() {
        stopping = true;
        if (drain != null) {
            try {
                drain.join(2000);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
            drain = null;
        }
        if (codec != null) {
            try {
                codec.stop();
            } catch (IllegalStateException ignored) {
                // Already stopped after an error.
            }
            codec.release();
            codec = null;
        }
        if (surface != null) {
            surface.release();
            surface = null;
        }
    }

    private void configure(MediaCodecInfo info, int width, int height) {
        int[] profiles = "hevc".equals(codecName)
                ? new int[] {MediaCodecInfo.CodecProfileLevel.HEVCProfileMain, -1}
                : new int[] {MediaCodecInfo.CodecProfileLevel.AVCProfileHigh,
                        MediaCodecInfo.CodecProfileLevel.AVCProfileMain, -1};
        RuntimeException last = null;
        for (int profile : profiles) {
            MediaFormat format = MediaFormat.createVideoFormat(mime, width, height);
            format.setInteger(MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
            format.setInteger(MediaFormat.KEY_BIT_RATE, bitrate);
            format.setInteger(MediaFormat.KEY_FRAME_RATE, maxFps);
            format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, I_FRAME_INTERVAL_SEC);
            format.setLong(MediaFormat.KEY_REPEAT_PREVIOUS_FRAME_AFTER, REPEAT_FRAME_US);
            format.setFloat(MediaFormat.KEY_MAX_FPS_TO_ENCODER, maxFps);
            format.setInteger(MediaFormat.KEY_PRIORITY, 0);
            // Every IDR carries SPS/PPS, so a desktop decoder that restarts mid-generation
            // (fallback, sync) can decode from the next keyframe. Encoders that lack it ignore it.
            format.setInteger(MediaFormat.KEY_PREPEND_HEADER_TO_SYNC_FRAMES, 1);
            if (profile >= 0) {
                if (!supportsProfile(info, profile)) {
                    continue;
                }
                format.setInteger(MediaFormat.KEY_PROFILE, profile);
            }
            try {
                codec.configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE);
                Ln.d("encoder profile " + (profile < 0 ? "default" : Integer.toString(profile)));
                return;
            } catch (RuntimeException e) {
                last = e;
                codec.reset();
            }
        }
        throw new IllegalStateException("encoder rejected every profile", last);
    }

    private boolean supportsProfile(MediaCodecInfo info, int profile) {
        for (MediaCodecInfo.CodecProfileLevel level : info.getCapabilitiesForType(mime).profileLevels) {
            if (level.profile == profile) {
                return true;
            }
        }
        return false;
    }

    private MediaCodecInfo chooseEncoder() {
        MediaCodecInfo fallback = null;
        for (MediaCodecInfo info : new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos()) {
            if (!info.isEncoder() || !supportsMime(info)) {
                continue;
            }
            if (info.isHardwareAccelerated()) {
                return info;
            }
            if (fallback == null) {
                fallback = info;
            }
        }
        if (fallback == null) {
            throw new IllegalStateException("no " + mime + " encoder on this device");
        }
        return fallback;
    }

    private boolean supportsMime(MediaCodecInfo info) {
        for (String type : info.getSupportedTypes()) {
            if (type.equalsIgnoreCase(mime)) {
                return true;
            }
        }
        return false;
    }

    private void drainLoop() {
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        try {
            while (!stopping) {
                int index = codec.dequeueOutputBuffer(info, 50_000);
                if (index < 0) {
                    continue;
                }
                ByteBuffer buffer = codec.getOutputBuffer(index);
                if (buffer != null && info.size > 0) {
                    int flags = 0;
                    if ((info.flags & MediaCodec.BUFFER_FLAG_CODEC_CONFIG) != 0) {
                        flags |= Frames.PACKET_FLAG_CONFIG;
                    }
                    if ((info.flags & MediaCodec.BUFFER_FLAG_KEY_FRAME) != 0) {
                        flags |= Frames.PACKET_FLAG_KEY;
                    }
                    buffer.position(info.offset);
                    buffer.limit(info.offset + info.size);
                    sink.onPacket(flags, info.presentationTimeUs, buffer);
                }
                codec.releaseOutputBuffer(index, false);
                if ((info.flags & MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                    break;
                }
            }
        } catch (IOException | IllegalStateException e) {
            if (!stopping) {
                sink.onEncoderFailure(e);
            }
        }
    }

    private static int align(int value, int alignment) {
        int aligned = (value / alignment) * alignment;
        return Math.max(alignment, aligned);
    }
}
