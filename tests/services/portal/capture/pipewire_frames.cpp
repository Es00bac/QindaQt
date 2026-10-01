// SPDX-License-Identifier: GPL-3.0-or-later
#include "pipewire_frames.h"
#include <spa/param/video/format-utils.h>
#include <spa/pod/builder.h>
#include <cstring>
#include <algorithm>
#include <unistd.h>
PipeWireFrames::PipeWireFrames(int fd, quint32 node) {
    pw_init(nullptr, nullptr); m_loop = pw_loop_new(nullptr);
    if (m_loop) pw_loop_enter(m_loop);
    if (m_loop) m_context = pw_context_new(m_loop, nullptr, 0);
    if (m_context) m_core = pw_context_connect_fd(m_context, fd, nullptr, 0); else close(fd);
    if (!m_core) { m_error = "actual remote connect failed"; return; }
    static const pw_core_events coreEvents = [] { pw_core_events v{}; v.version = PW_VERSION_CORE_EVENTS;
        v.error = [](void *p, uint32_t, int, int, const char *text) { static_cast<PipeWireFrames *>(p)->m_error = QString::fromUtf8(text); }; return v; }();
    pw_core_add_listener(m_core, &m_coreListener, &coreEvents, this);
    m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
    static const pw_registry_events registryEvents = [] { pw_registry_events v{}; v.version = PW_VERSION_REGISTRY_EVENTS;
        v.global = [](void *p, uint32_t id, uint32_t, const char *type, uint32_t, const spa_dict *) { if (strcmp(type, PW_TYPE_INTERFACE_Node) == 0) static_cast<PipeWireFrames *>(p)->m_nodes.insert(id); };
        v.global_remove = [](void *p, uint32_t id) { static_cast<PipeWireFrames *>(p)->m_nodes.remove(id); }; return v; }();
    pw_registry_add_listener(m_registry, &m_registryListener, &registryEvents, this);
    const auto id = QByteArray::number(node);
    m_stream = pw_stream_new(m_core, "private portal frame proof", pw_properties_new(PW_KEY_MEDIA_TYPE, "Video", PW_KEY_MEDIA_CATEGORY, "Capture", PW_KEY_MEDIA_ROLE, "Screen", PW_KEY_TARGET_OBJECT, id.constData(), nullptr));
    if (!m_stream) { m_error = "consumer stream creation failed"; return; }
    static const pw_stream_events streamEvents = [] { pw_stream_events v{}; v.version = PW_VERSION_STREAM_EVENTS;
        v.state_changed = [](void *p, pw_stream_state, pw_stream_state state, const char *error) { if (state == PW_STREAM_STATE_ERROR) static_cast<PipeWireFrames *>(p)->m_error = QString::fromUtf8(error); };
        v.param_changed = [](void *p, uint32_t idValue, const spa_pod *param) { if (param && idValue == SPA_PARAM_Format) spa_format_video_raw_parse(param, &static_cast<PipeWireFrames *>(p)->m_format); };
        v.process = process; return v; }();
    pw_stream_add_listener(m_stream, &m_listener, &streamEvents, this);
    uint8_t buffer[1024]; spa_pod_builder builder{}; spa_pod_builder_init(&builder, buffer, sizeof(buffer));
    const spa_rectangle size{1100, 820}, minimum{1, 1}, maximum{4096, 4096}; const spa_fraction rate{30, 1}, low{0, 1}, high{60, 1};
    const auto *format = static_cast<const spa_pod *>(spa_pod_builder_add_object(&builder, SPA_TYPE_OBJECT_Format, SPA_PARAM_EnumFormat,
        SPA_FORMAT_mediaType, SPA_POD_Id(SPA_MEDIA_TYPE_video), SPA_FORMAT_mediaSubtype, SPA_POD_Id(SPA_MEDIA_SUBTYPE_raw),
        SPA_FORMAT_VIDEO_format, SPA_POD_CHOICE_ENUM_Id(4, SPA_VIDEO_FORMAT_BGRA, SPA_VIDEO_FORMAT_BGRx, SPA_VIDEO_FORMAT_RGBA, SPA_VIDEO_FORMAT_RGBx),
        SPA_FORMAT_VIDEO_size, SPA_POD_CHOICE_RANGE_Rectangle(&size, &minimum, &maximum),
        SPA_FORMAT_VIDEO_framerate, SPA_POD_CHOICE_RANGE_Fraction(&rate, &low, &high)));
    const spa_pod *params[]{format};
    const auto flags = static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_DONT_RECONNECT);
    if (pw_stream_connect(m_stream, PW_DIRECTION_INPUT, node, flags, params, 1) < 0) { m_error = "actual node connection failed"; return; }
    m_poll.setInterval(5); connect(&m_poll, &QTimer::timeout, this, [this] { if (pw_loop_iterate(m_loop, 0) < 0) m_error = "private remote loop failed"; }); m_poll.start();
}
PipeWireFrames::~PipeWireFrames() {
    m_poll.stop(); if (m_stream) { spa_hook_remove(&m_listener); pw_stream_destroy(m_stream); }
    if (m_registry) { spa_hook_remove(&m_registryListener); pw_proxy_destroy(reinterpret_cast<pw_proxy *>(m_registry)); }
    if (m_core) { spa_hook_remove(&m_coreListener); pw_core_disconnect(m_core); }
    if (m_context) pw_context_destroy(m_context);
    if (m_loop) { pw_loop_leave(m_loop); pw_loop_destroy(m_loop); }
}
void PipeWireFrames::process(void *data) {
    auto &self = *static_cast<PipeWireFrames *>(data); auto *frame = pw_stream_dequeue_buffer(self.m_stream); if (!frame) return;
    const auto *buffer = frame->buffer;
    if (buffer && buffer->n_datas == 1 && buffer->datas[0].data && buffer->datas[0].chunk) {
        const auto &plane = buffer->datas[0]; const auto &chunk = *plane.chunk;
        const quint64 width = self.m_format.size.width, height = self.m_format.size.height;
        const bool rgba = self.m_format.format == SPA_VIDEO_FORMAT_RGBA || self.m_format.format == SPA_VIDEO_FORMAT_RGBx;
        const bool bgra = self.m_format.format == SPA_VIDEO_FORMAT_BGRA || self.m_format.format == SPA_VIDEO_FORMAT_BGRx;
        const quint64 length = height * static_cast<quint64>(std::max(chunk.stride, 0));
        if ((rgba || bgra) && width && height && width <= 4096 && height <= 4096 && chunk.stride >= static_cast<qint64>(width * 4) && length <= chunk.size && chunk.offset <= plane.maxsize && length <= plane.maxsize - chunk.offset) {
            const auto *bytes = static_cast<const uchar *>(plane.data) + chunk.offset;
            self.m_image = QImage(bytes, static_cast<int>(width), static_cast<int>(height), chunk.stride, rgba ? QImage::Format_RGBA8888 : QImage::Format_ARGB32).copy();
            quint64 hash = 1469598103934665603ULL;
            for (int y = 0; y < self.m_image.height(); y += 7) for (int x = 0; x < self.m_image.width(); x += 7) { hash ^= self.m_image.pixel(x, y); hash *= 1099511628211ULL; }
            if (self.m_checksums.size() < 128) self.m_checksums.insert(hash);
            ++self.m_frames;
        }
    }
    pw_stream_queue_buffer(self.m_stream, frame);
}
