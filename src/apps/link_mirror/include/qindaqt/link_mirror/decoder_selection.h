// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/link_mirror/mirror_messages.h"
#include "qindaqt/link_mirror/mirror_protocol.h"

#include <QImage>
#include <QList>
#include <QString>

#include <functional>
#include <memory>

// Video decoder abstraction and the VA-API-first selection policy.
//
// AGENT-NOTE: the policy lives here, apart from FFmpeg, so the fallback rules are unit-tested
// with fake decoders (tests/apps/link_mirror/tst_decoder_selection.cpp). ffmpeg_decoder.cpp is
// the only production VideoDecoder.
//
// Thread-safety: a DecoderSelector and its decoders are used from one thread (the decode
// worker thread).
namespace qindaqt::link_mirror {

enum class DecoderPath { Vaapi, Software };
enum class DecoderPreference { Auto, Vaapi, Software };

[[nodiscard]] QString decoderPathName(DecoderPath path);
[[nodiscard]] std::optional<DecoderPreference> parseDecoderPreference(const QString &text);

enum class DecodeResult { Frame, NoFrame, Error };

class VideoDecoder {
public:
    virtual ~VideoDecoder() = default;
    [[nodiscard]] virtual DecoderPath path() const = 0;
    // Prepares for a new stream generation. False means this path cannot decode it here.
    [[nodiscard]] virtual bool open(const VideoFormat &format, QString *error) = 0;
    // Feeds one packet. Frame fills *image with the newest picture (RGB32).
    [[nodiscard]] virtual DecodeResult decode(const VideoPacket &packet, QImage *image, QString *error) = 0;
};

using DecoderFactory = std::function<std::unique_ptr<VideoDecoder>(DecoderPath)>;

// Chooses and keeps a decoder for the stream.
//
// Rules (docs/wiki/reference/qindalink-mirror-v1.md, "Desktop decoding"):
// - Auto tries VA-API, then software. An explicit preference tries only that path.
// - A path that fails to open is skipped for this generation.
// - A hardware decode error falls back to software for the rest of the session; the caller
//   must request a keyframe because the software decoder starts without references.
// - A software decode error is reported and the next packet is still fed: one corrupt packet
//   must not end the session.
class DecoderSelector {
public:
    DecoderSelector(DecoderPreference preference, DecoderFactory factory);

    // Opens a decoder for a new stream generation (start or resize). False when no candidate
    // path could open; lastError() says why for each tried path.
    [[nodiscard]] bool open(const VideoFormat &format);
    [[nodiscard]] DecodeResult decode(const VideoPacket &packet, QImage *image);

    [[nodiscard]] bool isOpen() const { return m_decoder != nullptr; }
    [[nodiscard]] DecoderPath activePath() const { return m_path; }
    [[nodiscard]] QString lastError() const { return m_lastError; }
    // True once after a fallback; the caller sends a sync request and clears it.
    [[nodiscard]] bool takeKeyframeRequest();

private:
    [[nodiscard]] QList<DecoderPath> candidates() const;
    [[nodiscard]] bool openFirst(const VideoFormat &format);

    DecoderPreference m_preference;
    DecoderFactory m_factory;
    std::unique_ptr<VideoDecoder> m_decoder;
    DecoderPath m_path = DecoderPath::Software;
    VideoFormat m_format;
    bool m_hardwareDisabled = false;
    bool m_keyframeRequested = false;
    QString m_lastError;
};

} // namespace qindaqt::link_mirror
