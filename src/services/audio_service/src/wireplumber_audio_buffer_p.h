// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <spa/buffer/buffer.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace QindaQt::Audio::AudioBuffer
{

inline constexpr uint32_t kStereoStride = 2 * sizeof(float);

// AGENT-CONTRACT: meters, recording and VBAN negotiate interleaved F32 stereo.
// SPA chunk offset is modulo maxsize and size is bounded by maxsize. These
// borrowed views allocate nothing and live only until the buffer is requeued.
// Reject missing/misaligned planes instead of dereferencing producer metadata.
inline spa_data *stereoPlane(spa_buffer *buffer)
{
    if (buffer == nullptr || buffer->n_datas != 1 || buffer->datas == nullptr) {
        return nullptr;
    }
    spa_data &plane = buffer->datas[0];
    if (plane.data == nullptr || plane.chunk == nullptr
        || plane.maxsize < kStereoStride
        || reinterpret_cast<uintptr_t>(plane.data) % alignof(float) != 0) {
        return nullptr;
    }
    return &plane;
}

inline std::array<std::span<const float>, 2> capture(spa_buffer *buffer)
{
    const spa_data *plane = stereoPlane(buffer);
    if (plane == nullptr
        || (plane->chunk->flags & static_cast<int32_t>(SPA_CHUNK_FLAG_CORRUPTED)) != 0) {
        return {};
    }
    const uint32_t offset = plane->chunk->offset % plane->maxsize;
    if (offset % kStereoStride != 0
        || (plane->chunk->stride != 0
            && plane->chunk->stride != static_cast<int32_t>(kStereoStride))) {
        return {};
    }
    // A padded plane may end mid-frame; only an aligned plane can wrap
    // directly into another interleaved stereo span.
    const uint32_t capacity = plane->maxsize % kStereoStride == 0
        ? plane->maxsize : plane->maxsize - offset;
    const uint32_t bytes = std::min(plane->chunk->size, capacity)
        / kStereoStride * kStereoStride;
    const uint32_t first = std::min(bytes, plane->maxsize - offset);
    const auto *samples = static_cast<const float *>(plane->data);
    return {std::span<const float>(samples + offset / sizeof(float), first / sizeof(float)),
            std::span<const float>(samples, (bytes - first) / sizeof(float))};
}

inline std::span<float> playback(spa_buffer *buffer, uint64_t requestedFrames)
{
    spa_data *plane = stereoPlane(buffer);
    if (plane == nullptr) {
        if (buffer != nullptr && buffer->n_datas > 0 && buffer->datas != nullptr
            && buffer->datas[0].chunk != nullptr) {
            buffer->datas[0].chunk->size = 0;
        }
        return {};
    }
    const uint64_t capacity = plane->maxsize / kStereoStride;
    const auto frames = static_cast<uint32_t>(requestedFrames == 0
        ? capacity : std::min(capacity, requestedFrames));
    plane->chunk->offset = 0;
    plane->chunk->stride = static_cast<int32_t>(kStereoStride);
    plane->chunk->size = frames * kStereoStride;
    plane->chunk->flags = SPA_CHUNK_FLAG_NONE;
    return {static_cast<float *>(plane->data), static_cast<size_t>(frames) * 2};
}

} // namespace QindaQt::Audio::AudioBuffer
