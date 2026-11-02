// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/link_mirror/decoder_selection.h"

#include <utility>

namespace qindaqt::link_mirror {

QString decoderPathName(DecoderPath path)
{
    return path == DecoderPath::Vaapi ? QStringLiteral("vaapi") : QStringLiteral("software");
}

std::optional<DecoderPreference> parseDecoderPreference(const QString &text)
{
    if (text == QLatin1String("auto")) {
        return DecoderPreference::Auto;
    }
    if (text == QLatin1String("vaapi")) {
        return DecoderPreference::Vaapi;
    }
    if (text == QLatin1String("software")) {
        return DecoderPreference::Software;
    }
    return std::nullopt;
}

DecoderSelector::DecoderSelector(DecoderPreference preference, DecoderFactory factory)
    : m_preference(preference), m_factory(std::move(factory))
{
}

QList<DecoderPath> DecoderSelector::candidates() const
{
    switch (m_preference) {
    case DecoderPreference::Vaapi:
        return {DecoderPath::Vaapi};
    case DecoderPreference::Software:
        return {DecoderPath::Software};
    case DecoderPreference::Auto:
        break;
    }
    if (m_hardwareDisabled) {
        return {DecoderPath::Software};
    }
    return {DecoderPath::Vaapi, DecoderPath::Software};
}

bool DecoderSelector::open(const VideoFormat &format)
{
    m_format = format;
    m_config = VideoPacket();
    return openFirst(format);
}

bool DecoderSelector::openFirst(const VideoFormat &format)
{
    m_decoder.reset();
    QStringList failures;
    for (const DecoderPath path : candidates()) {
        std::unique_ptr<VideoDecoder> decoder = m_factory(path);
        QString error;
        if (decoder && decoder->open(format, &error)) {
            m_decoder = std::move(decoder);
            m_path = path;
            m_lastError = failures.join(QStringLiteral("; "));
            return true;
        }
        failures.append(decoderPathName(path) + QStringLiteral(": ")
                        + (error.isEmpty() ? QStringLiteral("unavailable") : error));
    }
    m_lastError = failures.join(QStringLiteral("; "));
    return false;
}

DecodeResult DecoderSelector::decode(const VideoPacket &packet, QImage *image)
{
    if (!m_decoder) {
        return DecodeResult::Error;
    }
    if (packet.config) {
        m_config = packet;
    }
    QString error;
    const DecodeResult result = m_decoder->decode(packet, image, &error);
    if (result != DecodeResult::Error) {
        return result;
    }
    m_lastError = decoderPathName(m_path) + QStringLiteral(" decode: ") + error;
    if (m_path == DecoderPath::Vaapi && m_preference == DecoderPreference::Auto) {
        // AGENT-GUARD: once hardware has failed mid-stream, stay on software for the session;
        // flapping between paths would drop a keyframe interval of video each time.
        m_hardwareDisabled = true;
        const QString hardwareError = m_lastError;
        if (openFirst(m_format)) {
            m_keyframeRequested = true;
            m_lastError = hardwareError;
            if (!m_config.data.isEmpty()) {
                QImage ignored;
                QString replayError;
                // AGENT-GUARD: without the replayed parameter sets the fresh decoder can never
                // decode this generation, since its later keyframes may carry no SPS/PPS.
                (void)m_decoder->decode(m_config, &ignored, &replayError);
            }
        }
    }
    return DecodeResult::Error;
}

bool DecoderSelector::takeKeyframeRequest()
{
    return std::exchange(m_keyframeRequested, false);
}

BacklogGate::Verdict BacklogGate::admit(const VideoPacket &packet, int inFlight)
{
    if (packet.config) {
        return Verdict::Queue;
    }
    if (packet.key) {
        m_dropping = false;
        return Verdict::Queue;
    }
    if (m_dropping) {
        return Verdict::Drop;
    }
    if (inFlight >= m_limit) {
        m_dropping = true;
        return Verdict::DropAndRequestKeyframe;
    }
    return Verdict::Queue;
}

} // namespace qindaqt::link_mirror
