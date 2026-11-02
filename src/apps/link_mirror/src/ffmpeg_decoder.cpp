// SPDX-License-Identifier: GPL-3.0-or-later
#include "ffmpeg_decoder.h"

#include <QSysInfo>

#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/log.h>
#include <libswscale/swscale.h>
}

namespace qindaqt::link_mirror {
namespace {

// QImage::Format_RGB32 is 0xffRRGGBB in native byte order.
constexpr AVPixelFormat kRgb32 = QSysInfo::ByteOrder == QSysInfo::LittleEndian ? AV_PIX_FMT_BGRA : AV_PIX_FMT_ARGB;

QString averror(int code)
{
    char text[AV_ERROR_MAX_STRING_SIZE] = {};
    av_strerror(code, text, sizeof text);
    return QString::fromLocal8Bit(text);
}

class FfmpegDecoder final : public VideoDecoder {
public:
    explicit FfmpegDecoder(DecoderPath path) : m_path(path) {}
    ~FfmpegDecoder() override { close(); }
    FfmpegDecoder(const FfmpegDecoder &) = delete;
    FfmpegDecoder &operator=(const FfmpegDecoder &) = delete;

    [[nodiscard]] DecoderPath path() const override { return m_path; }

    bool open(const VideoFormat &format, QString *error) override
    {
        close();
        const AVCodecID id = format.codec == QLatin1String("hevc") ? AV_CODEC_ID_HEVC : AV_CODEC_ID_H264;
        const AVCodec *codec = avcodec_find_decoder(id);
        if (codec == nullptr) {
            *error = QStringLiteral("libavcodec has no %1 decoder").arg(format.codec);
            return false;
        }
        m_context = avcodec_alloc_context3(codec);
        m_packet = av_packet_alloc();
        m_frame = av_frame_alloc();
        m_transfer = av_frame_alloc();
        if (m_context == nullptr || m_packet == nullptr || m_frame == nullptr || m_transfer == nullptr) {
            *error = QStringLiteral("out of memory");
            return false;
        }
        m_context->flags |= AV_CODEC_FLAG_LOW_DELAY;
        m_context->thread_type = FF_THREAD_SLICE;
        m_context->opaque = this;
        if (m_path == DecoderPath::Vaapi) {
            // nullptr device: libva picks the default render node (radeonsi on qinda).
            const int rc = av_hwdevice_ctx_create(&m_device, AV_HWDEVICE_TYPE_VAAPI, nullptr, nullptr, 0);
            if (rc < 0) {
                *error = QStringLiteral("VA-API device: %1").arg(averror(rc));
                return false;
            }
            m_context->hw_device_ctx = av_buffer_ref(m_device);
            m_context->get_format = &FfmpegDecoder::chooseFormat;
        }
        const int rc = avcodec_open2(m_context, codec, nullptr);
        if (rc < 0) {
            *error = QStringLiteral("avcodec_open2: %1").arg(averror(rc));
            return false;
        }
        return true;
    }

    DecodeResult decode(const VideoPacket &packet, QImage *image, QString *error) override
    {
        // AGENT-NOTE: MediaCodec emits SPS/PPS as a separate config buffer. Holding it and
        // prepending it to the next packet (a keyframe) keeps each decode call self-contained.
        if (packet.config) {
            m_pendingConfig = packet.data;
            return DecodeResult::NoFrame;
        }
        const QByteArray data = m_pendingConfig.isEmpty() ? packet.data : m_pendingConfig + packet.data;
        m_pendingConfig.clear();
        av_packet_unref(m_packet);
        if (av_new_packet(m_packet, static_cast<int>(data.size())) < 0) {
            *error = QStringLiteral("out of memory");
            return DecodeResult::Error;
        }
        std::memcpy(m_packet->data, data.constData(), static_cast<size_t>(data.size()));
        m_packet->pts = packet.ptsUs;
        if (packet.key) {
            m_packet->flags |= AV_PKT_FLAG_KEY;
        }
        int rc = avcodec_send_packet(m_context, m_packet);
        if (rc < 0 && rc != AVERROR(EAGAIN)) {
            *error = QStringLiteral("send: %1").arg(averror(rc));
            return DecodeResult::Error;
        }
        bool produced = false;
        while ((rc = avcodec_receive_frame(m_context, m_frame)) >= 0) {
            const bool ok = convert(m_frame, image, error);
            av_frame_unref(m_frame);
            if (!ok) {
                return DecodeResult::Error;
            }
            produced = true;
        }
        if (m_hardwareRejected) {
            *error = QStringLiteral("VA-API does not support this stream");
            return DecodeResult::Error;
        }
        if (rc != AVERROR(EAGAIN) && rc != AVERROR_EOF) {
            *error = QStringLiteral("receive: %1").arg(averror(rc));
            return DecodeResult::Error;
        }
        return produced ? DecodeResult::Frame : DecodeResult::NoFrame;
    }

private:
    static AVPixelFormat chooseFormat(AVCodecContext *context, const AVPixelFormat *formats)
    {
        for (const AVPixelFormat *format = formats; *format != AV_PIX_FMT_NONE; ++format) {
            if (*format == AV_PIX_FMT_VAAPI) {
                return *format;
            }
        }
        // AGENT-GUARD: never let libav fall back to software silently on the VA-API path;
        // --probe reports the path, and that report must be true.
        static_cast<FfmpegDecoder *>(context->opaque)->m_hardwareRejected = true;
        return AV_PIX_FMT_NONE;
    }

    bool convert(AVFrame *frame, QImage *image, QString *error)
    {
        AVFrame *source = frame;
        if (frame->format == AV_PIX_FMT_VAAPI) {
            av_frame_unref(m_transfer);
            const int rc = av_hwframe_transfer_data(m_transfer, frame, 0);
            if (rc < 0) {
                *error = QStringLiteral("VA-API download: %1").arg(averror(rc));
                return false;
            }
            source = m_transfer;
        }
        const auto format = static_cast<AVPixelFormat>(source->format);
        m_scaler = sws_getCachedContext(m_scaler, source->width, source->height, format, source->width,
                                        source->height, kRgb32, SWS_BILINEAR, nullptr, nullptr, nullptr);
        if (m_scaler == nullptr) {
            *error = QStringLiteral("no RGB conversion for pixel format %1").arg(static_cast<int>(format));
            return false;
        }
        const int colorspace = source->colorspace == AVCOL_SPC_BT709 ? SWS_CS_ITU709 : SWS_CS_ITU601;
        const int fullRange = source->color_range == AVCOL_RANGE_JPEG ? 1 : 0;
        sws_setColorspaceDetails(m_scaler, sws_getCoefficients(colorspace), fullRange, sws_getCoefficients(colorspace),
                                 1, 0, 1 << 16, 1 << 16);
        QImage converted(source->width, source->height, QImage::Format_RGB32);
        uint8_t *destination[1] = {converted.bits()};
        const int stride[1] = {static_cast<int>(converted.bytesPerLine())};
        sws_scale(m_scaler, source->data, source->linesize, 0, source->height, destination, stride);
        *image = std::move(converted);
        return true;
    }

    void close()
    {
        avcodec_free_context(&m_context);
        av_buffer_unref(&m_device);
        av_packet_free(&m_packet);
        av_frame_free(&m_frame);
        av_frame_free(&m_transfer);
        sws_freeContext(m_scaler);
        m_scaler = nullptr;
        m_pendingConfig.clear();
        m_hardwareRejected = false;
    }

    DecoderPath m_path;
    AVCodecContext *m_context = nullptr;
    AVBufferRef *m_device = nullptr;
    AVPacket *m_packet = nullptr;
    AVFrame *m_frame = nullptr;
    AVFrame *m_transfer = nullptr;
    SwsContext *m_scaler = nullptr;
    QByteArray m_pendingConfig;
    bool m_hardwareRejected = false;
};

} // namespace

std::unique_ptr<VideoDecoder> makeFfmpegDecoder(DecoderPath path)
{
    return std::make_unique<FfmpegDecoder>(path);
}

void quietFfmpegLogging(bool verbose)
{
    av_log_set_level(verbose ? AV_LOG_WARNING : AV_LOG_FATAL);
}

} // namespace qindaqt::link_mirror
