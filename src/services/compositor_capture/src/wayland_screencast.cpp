// SPDX-License-Identifier: GPL-3.0-or-later
#include <qindaqt/services/compositor_capture/wayland_screencast.h>
#include "screencast-client.h"
#include <QSocketNotifier>
#include <QTimer>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <wayland-client.h>
namespace QindaQt::CompositorCapture {
class WaylandScreenCast::Private {
public:
    struct Output { Private *self; uint32_t global; wl_output *proxy; MonitorSource source; QSize pixels; int scale = 1; bool complete = false; };
    WaylandScreenCast &q;
    std::function<bool()> admission;
    wl_display *display = nullptr; wl_registry *registry = nullptr; wl_callback *sync = nullptr;
    zkde_screencast_unstable_v1 *manager = nullptr; zkde_screencast_stream_unstable_v1 *stream = nullptr;
    std::unique_ptr<QSocketNotifier> reading, writing;
    std::vector<std::unique_ptr<Output>> outputs;
    QTimer deadline, lineage;
    Output *selected = nullptr;
    uint32_t managerGlobal = 0;
    bool ready = false, invalid = false, dispatching = false, started = false, created = false;
    explicit Private(WaylandScreenCast &object, std::function<bool()> admitted) : q(object), admission(std::move(admitted)) {
        deadline.setSingleShot(true); deadline.setInterval(4000);
        QObject::connect(&deadline, &QTimer::timeout, &q, [this] { close(); });
        lineage.setInterval(50);
        QObject::connect(&lineage, &QTimer::timeout, &q, [this] { if (!live()) close(); });
    }
    ~Private() { teardown(); }
    bool live() const { return !invalid && admission && admission(); }
    void teardown() {
        reading.reset(); writing.reset();
        if (stream) zkde_screencast_stream_unstable_v1_close(stream);
        stream = nullptr;
        if (sync) wl_callback_destroy(sync);
        sync = nullptr;
        if (manager) zkde_screencast_unstable_v1_destroy(manager);
        manager = nullptr;
        for (auto &output : outputs) wl_output_release(output->proxy);
        outputs.clear(); selected = nullptr;
        if (registry) wl_registry_destroy(registry);
        registry = nullptr;
        if (display) { wl_display_flush(display); wl_display_disconnect(display); }
        display = nullptr;
    }
    void close() {
        if (invalid) return;
        invalid = true; ready = false; deadline.stop(); lineage.stop();
        // Invalidate synchronously, but never free listener contexts in dispatch.
        QTimer::singleShot(0, &q, [this] { teardown(); Q_EMIT q.closed(); });
    }
    void flush() {
        if (!display || invalid) return;
        if (wl_display_flush(display) < 0) {
            if (errno != EAGAIN) close(); else writing->setEnabled(true);
        } else writing->setEnabled(false);
    }
    void dispatch() {
        if (!display || dispatching || !live()) { close(); return; }
        while (wl_display_prepare_read(display) != 0) {
            dispatching = true; const int result = wl_display_dispatch_pending(display); dispatching = false;
            if (result < 0 || invalid || !live()) { close(); return; }
        }
        if (wl_display_read_events(display) < 0) { close(); return; }
        dispatching = true; const int result = wl_display_dispatch_pending(display); dispatching = false;
        if (result < 0 || !live()) { close(); return; }
        flush();
    }
    static void global(void *data, wl_registry *reg, uint32_t name, const char *interface, uint32_t version) {
        auto &self = *static_cast<Private *>(data);
        if (!self.live()) { self.close(); return; }
        if (strcmp(interface, zkde_screencast_unstable_v1_interface.name) == 0) {
            if (self.manager) { self.close(); return; }
            self.managerGlobal = name;
            self.manager = static_cast<zkde_screencast_unstable_v1 *>(wl_registry_bind(reg, name, &zkde_screencast_unstable_v1_interface, std::min(version, 5U)));
        } else if (strcmp(interface, wl_output_interface.name) == 0) {
            if (version < 4 || self.outputs.size() >= 16 || self.ready) { self.close(); return; }
            auto output = std::make_unique<Output>(); output->self = &self; output->global = name;
            output->source.id = QString::number(name);
            output->proxy = static_cast<wl_output *>(wl_registry_bind(reg, name, &wl_output_interface, 4));
            static const wl_output_listener events{
                [](void *d, wl_output *, int32_t x, int32_t y, int32_t, int32_t, int32_t, const char *, const char *, int32_t) { static_cast<Output *>(d)->source.position = {x, y}; },
                [](void *d, wl_output *, uint32_t flags, int32_t w, int32_t h, int32_t) { if (flags & WL_OUTPUT_MODE_CURRENT) static_cast<Output *>(d)->pixels = {w, h}; },
                [](void *d, wl_output *) { auto &o = *static_cast<Output *>(d); o.source.size = o.pixels / o.scale; o.complete = true; },
                [](void *d, wl_output *, int32_t scale) { auto &o = *static_cast<Output *>(d); if (scale < 1 || scale > 8) o.self->close(); else o.scale = scale; },
                [](void *d, wl_output *, const char *nameText) { auto &o = *static_cast<Output *>(d); o.source.name = QString::fromUtf8(nameText); if (o.source.name.size() > 256) o.self->close(); },
                [](void *, wl_output *, const char *) {}};
            wl_output_add_listener(output->proxy, &events, output.get());
            self.outputs.push_back(std::move(output));
        }
    }
    void open(int fd) {
        if (fd < 0 || !live()) { if (fd >= 0) ::close(fd); close(); return; }
        display = wl_display_connect_to_fd(fd);
        if (!display) { close(); return; } // libwayland consumes fd on failure too.
        reading = std::make_unique<QSocketNotifier>(wl_display_get_fd(display), QSocketNotifier::Read, &q);
        writing = std::make_unique<QSocketNotifier>(wl_display_get_fd(display), QSocketNotifier::Write, &q); writing->setEnabled(false);
        QObject::connect(reading.get(), &QSocketNotifier::activated, &q, [this] { dispatch(); });
        QObject::connect(writing.get(), &QSocketNotifier::activated, &q, [this] { flush(); });
        registry = wl_display_get_registry(display);
        static const wl_registry_listener events{global, [](void *d, wl_registry *, uint32_t name) {
            auto &self = *static_cast<Private *>(d);
            if (name == self.managerGlobal) self.close();
            for (const auto &o : self.outputs) if (o->global == name) self.close();
        }};
        wl_registry_add_listener(registry, &events, this);
        // A second barrier includes events from globals bound by the first batch.
        sync = wl_display_sync(display);
        static const wl_callback_listener first{[](void *d, wl_callback *callback, uint32_t) {
            auto &self = *static_cast<Private *>(d); wl_callback_destroy(callback);
            self.sync = wl_display_sync(self.display);
            static const wl_callback_listener second{[](void *data, wl_callback *cb, uint32_t) {
                auto &s = *static_cast<Private *>(data); wl_callback_destroy(cb); s.sync = nullptr;
                if (!s.live() || !s.manager || s.outputs.empty()) { s.close(); return; }
                for (const auto &o : s.outputs) if (!o->complete || o->source.name.isEmpty() || !o->source.size.isValid() || o->pixels.width() > 16384 || o->pixels.height() > 16384) { s.close(); return; }
                s.ready = true; s.deadline.stop(); QTimer::singleShot(0, &s.q, [&s] { if (s.live()) Q_EMIT s.q.sourcesReady(); });
            }};
            wl_callback_add_listener(self.sync, &second, &self);
        }};
        wl_callback_add_listener(sync, &first, this); deadline.start(); lineage.start(); flush();
    }
};
WaylandScreenCast::WaylandScreenCast(int fd, std::function<bool()> admission, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(*this, std::move(admission))) { d->open(fd); }
WaylandScreenCast::~WaylandScreenCast() = default;
QList<MonitorSource> WaylandScreenCast::sources() const {
    QList<MonitorSource> result;
    if (d->ready && d->live()) for (const auto &o : d->outputs) result.append(o->source);
    return result;
}
bool WaylandScreenCast::start(const QString &id) {
    if (!d->ready || !d->live() || d->started) return false;
    for (const auto &o : d->outputs) if (o->source.id == id) d->selected = o.get();
    if (!d->selected) return false;
    d->started = true;
    d->stream = zkde_screencast_unstable_v1_stream_output(d->manager, d->selected->proxy, ZKDE_SCREENCAST_UNSTABLE_V1_POINTER_HIDDEN);
    static const zkde_screencast_stream_unstable_v1_listener events{
        [](void *data, zkde_screencast_stream_unstable_v1 *) { static_cast<Private *>(data)->close(); },
        [](void *data, zkde_screencast_stream_unstable_v1 *, uint32_t node) {
            auto &s = *static_cast<Private *>(data);
            if (!s.live() || s.created || !node || !s.selected) { s.close(); return; }
            s.created = true; s.deadline.stop(); const auto source = s.selected->source;
            QTimer::singleShot(0, &s.q, [&s, node, source] { if (s.live()) Q_EMIT s.q.streamCreated(node, source); });
        },
        [](void *data, zkde_screencast_stream_unstable_v1 *, const char *) { static_cast<Private *>(data)->close(); },
        [](void *, zkde_screencast_stream_unstable_v1 *, uint32_t, uint32_t) {}};
    zkde_screencast_stream_unstable_v1_add_listener(d->stream, &events, d.get()); d->deadline.start(); d->flush(); return true;
}
void WaylandScreenCast::stop() { d->close(); }
}
