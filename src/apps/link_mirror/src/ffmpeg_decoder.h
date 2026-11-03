// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/link_mirror/decoder_selection.h"

#include <memory>

// libavcodec implementation of VideoDecoder for the VA-API and software paths.
//
// AGENT-NOTE: the VA-API path decodes on the GPU and downloads each picture to system memory
// before RGB conversion; the presenter is a raster window, so a zero-copy path would add an
// EGL/dmabuf import for little gain at phone resolutions. Revisit if 4K mirrors matter.
namespace qindaqt::link_mirror {

// Returns a decoder for the path; never null. Opening reports whether the path works here.
[[nodiscard]] std::unique_ptr<VideoDecoder> makeFfmpegDecoder(DecoderPath path);

// Silences libav's per-packet warnings (missing references before the first keyframe are
// expected while a stream starts). Call once per process.
void quietFfmpegLogging(bool verbose);

} // namespace qindaqt::link_mirror
