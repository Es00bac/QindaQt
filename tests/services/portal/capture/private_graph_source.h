// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <pipewire/pipewire.h>
#include <spa/param/video/format-utils.h>
#include <spa/param/buffers.h>
#include <spa/pod/builder.h>
#include <QTimer>
#include <array>
#include <cstring>

// Task-only actual PipeWire producer. The graph's offered and default sources
// have distinct pixels; decoder and native-capture fixtures remain unchanged.
class PrivateGraphSource final {
public:
    PrivateGraphSource(pw_core *core, const char *name, bool unrelated)
        : pixel(unrelated ? std::array<unsigned char, 4>{0, 0, 255, 255}
                          : std::array<unsigned char, 4>{255, 0, 0, 255}) {
        stream = pw_stream_new(core, name, pw_properties_new(PW_KEY_NODE_NAME, name,
            PW_KEY_MEDIA_CLASS, "Video/Source", PW_KEY_MEDIA_TYPE, "Video",
            "node.virtual", "true", nullptr));
        if (!stream) return;
        static const pw_stream_events events = [] {
            pw_stream_events e{}; e.version = PW_VERSION_STREAM_EVENTS;
            e.param_changed = [](void *data, uint32_t id, const spa_pod *param) {
                if (id != SPA_PARAM_Format || !param) return;
                auto &self = *static_cast<PrivateGraphSource *>(data);
                uint8_t storage[256]; spa_pod_builder builder{};
                spa_pod_builder_init(&builder, storage, sizeof(storage));
                const auto *buffers = static_cast<const spa_pod *>(spa_pod_builder_add_object(&builder,
                    SPA_TYPE_OBJECT_ParamBuffers, SPA_PARAM_Buffers,
                    SPA_PARAM_BUFFERS_buffers, SPA_POD_CHOICE_RANGE_Int(4, 2, 8),
                    SPA_PARAM_BUFFERS_blocks, SPA_POD_Int(1),
                    SPA_PARAM_BUFFERS_size, SPA_POD_Int(bytes),
                    SPA_PARAM_BUFFERS_stride, SPA_POD_Int(stride),
                    SPA_PARAM_BUFFERS_dataType, SPA_POD_CHOICE_FLAGS_Int(1 << SPA_DATA_MemFd)));
                const spa_pod *params[]{buffers}; pw_stream_update_params(self.stream, params, 1);
            };
            e.process = [](void *data) {
                auto &self = *static_cast<PrivateGraphSource *>(data);
                auto *frame = pw_stream_dequeue_buffer(self.stream); if (!frame) return;
                auto *buffer = frame->buffer;
                if (buffer && buffer->n_datas == 1) {
                    auto &plane = buffer->datas[0];
                    if (plane.data && plane.chunk && plane.maxsize >= bytes) {
                        auto *out = static_cast<unsigned char *>(plane.data);
                        for (int offset = 0; offset < bytes; offset += 4) memcpy(out + offset, self.pixel.data(), 4);
                        plane.chunk->offset = 0; plane.chunk->size = bytes; plane.chunk->stride = stride;
                    }
                }
                pw_stream_queue_buffer(self.stream, frame);
            }; return e;
        }();
        pw_stream_add_listener(stream, &listener, &events, this);
        uint8_t storage[256]; spa_pod_builder builder{};
        spa_pod_builder_init(&builder, storage, sizeof(storage));
        spa_video_info_raw info{}; info.format = SPA_VIDEO_FORMAT_RGBA;
        info.size = {320, 240}; info.framerate = {30, 1};
        const spa_pod *params[]{spa_format_video_raw_build(&builder, SPA_PARAM_EnumFormat, &info)};
        connected = pw_stream_connect(stream, PW_DIRECTION_OUTPUT, PW_ID_ANY,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_DRIVER | PW_STREAM_FLAG_MAP_BUFFERS), params, 1) >= 0;
        // This timer is the producer's frame clock, not a readiness delay. Only
        // PipeWire's actual STREAMING + driver state permits a graph cycle.
        clock.setInterval(33);
        QObject::connect(&clock, &QTimer::timeout, [&] {
            if (pw_stream_get_state(stream, nullptr) == PW_STREAM_STATE_STREAMING && pw_stream_is_driving(stream))
                pw_stream_trigger_process(stream);
        });
        clock.start();
    }
    ~PrivateGraphSource() { clock.stop(); if (stream) { spa_hook_remove(&listener); pw_stream_destroy(stream); } }
    bool valid() const { return connected; }
private:
    static constexpr int stride = 320 * 4, bytes = stride * 240;
    pw_stream *stream = nullptr; spa_hook listener{}; QTimer clock;
    std::array<unsigned char, 4> pixel; bool connected = false;
};
