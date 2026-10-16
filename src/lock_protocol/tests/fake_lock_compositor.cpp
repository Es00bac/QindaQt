// SPDX-License-Identifier: GPL-3.0-or-later
#include "fake_lock_compositor.h"
#include "ext-session-lock-v1-server-protocol.h"
#include <wayland-server.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
namespace QindaQt::LockProtocol::Testing {
using Kind = FakeLockCompositor::Observation::Kind;
struct LockCompositorState;
struct Surface {
  LockCompositorState *state = nullptr;
  wl_resource *resource = nullptr;
  wl_resource *pendingBuffer = nullptr;
  std::vector<wl_resource *> frames;
};
struct LockCompositorState {
  using Observation = FakeLockCompositor::Observation;
  wl_display *display = nullptr;
  wl_event_source *wake = nullptr;
  int eventFd = -1;
  std::thread thread;
  std::mutex mutex;
  std::queue<std::function<void()>> commands;
  std::vector<Observation> seen;
  wl_resource *lock = nullptr, *lockSurface = nullptr;
  std::uint32_t lastSerial = 0;

  void record(Observation observation) {
    std::lock_guard<std::mutex> guard(mutex);
    seen.push_back(observation);
  }
  // Runs `command` on the compositor thread and waits for it.
  template <typename Result> Result run(std::function<Result()> command) {
    auto task = std::make_shared<std::packaged_task<Result()>>(std::move(command));
    auto result = task->get_future();
    { std::lock_guard<std::mutex> guard(mutex); commands.push([task] { (*task)(); }); }
    const std::uint64_t one = 1;
    if (write(eventFd, &one, sizeof(one)) < 0) return Result();
    return result.get();
  }
  static int drain(int fd, std::uint32_t, void *data) {
    auto *self = static_cast<LockCompositorState *>(data);
    std::uint64_t count = 0;
    if (read(fd, &count, sizeof(count)) < 0) return 0;
    for (;;) {
      std::function<void()> next;
      { std::lock_guard<std::mutex> guard(self->mutex);
        if (self->commands.empty()) break;
        next = std::move(self->commands.front());
        self->commands.pop(); }
      next();
    }
    wl_display_flush_clients(self->display);
    return 0;
  }
};
namespace {
LockCompositorState *impl(wl_resource *resource) {
  return static_cast<LockCompositorState *>(wl_resource_get_user_data(resource));
}
// --- wl_region: never consulted, only accepted. ---
const struct wl_region_interface regionImpl{
    [](wl_client *, wl_resource *r) { wl_resource_destroy(r); },
    [](wl_client *, wl_resource *, int32_t, int32_t, int32_t, int32_t) {},
    [](wl_client *, wl_resource *, int32_t, int32_t, int32_t, int32_t) {}};
// --- wl_surface: records buffer sizes at commit and completes frame callbacks. ---
const struct wl_surface_interface surfaceImpl{
    [](wl_client *, wl_resource *r) { wl_resource_destroy(r); },
    [](wl_client *, wl_resource *r, wl_resource *buffer, int32_t, int32_t) {
      static_cast<Surface *>(wl_resource_get_user_data(r))->pendingBuffer = buffer;
    },
    [](wl_client *, wl_resource *, int32_t, int32_t, int32_t, int32_t) {},
    [](wl_client *client, wl_resource *r, uint32_t id) {
      auto *surface = static_cast<Surface *>(wl_resource_get_user_data(r));
      wl_resource *callback = wl_resource_create(client, &wl_callback_interface, 1, id);
      if (!callback) { wl_client_post_no_memory(client); return; }
      wl_resource_set_implementation(callback, nullptr, nullptr, nullptr);
      surface->frames.push_back(callback);
    },
    [](wl_client *, wl_resource *, wl_resource *) {},
    [](wl_client *, wl_resource *, wl_resource *) {},
    [](wl_client *, wl_resource *r) {
      auto *surface = static_cast<Surface *>(wl_resource_get_user_data(r));
      auto *state = surface->state;
      if (surface->pendingBuffer) {
        if (auto *shm = wl_shm_buffer_get(surface->pendingBuffer)) {
          state->record({Kind::Commit, 0, wl_shm_buffer_get_width(shm), wl_shm_buffer_get_height(shm)});
        }
        wl_buffer_send_release(surface->pendingBuffer);
        surface->pendingBuffer = nullptr;
      }
      for (auto *callback : surface->frames) { wl_callback_send_done(callback, 0); wl_resource_destroy(callback); }
      surface->frames.clear();
    },
    [](wl_client *, wl_resource *, int32_t) {},
    [](wl_client *, wl_resource *, int32_t) {},
    [](wl_client *, wl_resource *, int32_t, int32_t, int32_t, int32_t) {},
    [](wl_client *, wl_resource *, int32_t, int32_t) {}}; // offset (v5), never used at version 4
const struct wl_compositor_interface compositorImpl{
    [](wl_client *client, wl_resource *r, uint32_t id) {
      auto *surface = new Surface;
      surface->state = impl(r);
      surface->resource = wl_resource_create(client, &wl_surface_interface, wl_resource_get_version(r), id);
      if (!surface->resource) { delete surface; wl_client_post_no_memory(client); return; }
      wl_resource_set_implementation(surface->resource, &surfaceImpl, surface,
                                     [](wl_resource *dead) { delete static_cast<Surface *>(wl_resource_get_user_data(dead)); });
    },
    [](wl_client *client, wl_resource *r, uint32_t id) {
      wl_resource *region = wl_resource_create(client, &wl_region_interface, wl_resource_get_version(r), id);
      if (!region) { wl_client_post_no_memory(client); return; }
      wl_resource_set_implementation(region, &regionImpl, nullptr, nullptr);
    }};
// --- wl_output: one 1280x1024 output. ---
const struct wl_output_interface outputImpl{[](wl_client *, wl_resource *r) { wl_resource_destroy(r); }};
// --- ext-session-lock-v1 ---
const struct ext_session_lock_surface_v1_interface lockSurfaceImpl{
    [](wl_client *, wl_resource *r) { wl_resource_destroy(r); },
    [](wl_client *, wl_resource *r, uint32_t serial) { impl(r)->record({Kind::AckConfigure, serial, 0, 0}); }};
const struct ext_session_lock_v1_interface lockImpl{
    [](wl_client *, wl_resource *r) { wl_resource_destroy(r); },
    [](wl_client *client, wl_resource *r, uint32_t id, wl_resource *, wl_resource *) {
      auto *state = impl(r);
      wl_resource *surface = wl_resource_create(client, &ext_session_lock_surface_v1_interface,
                                                wl_resource_get_version(r), id);
      if (!surface) { wl_client_post_no_memory(client); return; }
      wl_resource_set_implementation(surface, &lockSurfaceImpl, state, [](wl_resource *dead) {
        auto *self = impl(dead);
        if (self->lockSurface == dead) self->lockSurface = nullptr;
        self->record({Kind::LockSurfaceDestroyed, 0, 0, 0});
      });
      state->lockSurface = surface;
      state->record({Kind::LockSurfaceRequested, 0, 0, 0});
    },
    [](wl_client *, wl_resource *r) {
      impl(r)->record({Kind::UnlockRequested, 0, 0, 0});
      wl_resource_destroy(r);
    }};
const struct ext_session_lock_manager_v1_interface managerImpl{
    [](wl_client *, wl_resource *r) { wl_resource_destroy(r); },
    [](wl_client *client, wl_resource *r, uint32_t id) {
      auto *state = impl(r);
      wl_resource *lock = wl_resource_create(client, &ext_session_lock_v1_interface, wl_resource_get_version(r), id);
      if (!lock) { wl_client_post_no_memory(client); return; }
      wl_resource_set_implementation(lock, &lockImpl, state, [](wl_resource *dead) {
        auto *self = impl(dead);
        if (self->lock == dead) self->lock = nullptr;
      });
      state->lock = lock;
      state->record({Kind::LockRequested, 0, 0, 0});
    }};
} // namespace

FakeLockCompositor::FakeLockCompositor() : m_impl(std::make_unique<LockCompositorState>()) {}
FakeLockCompositor::~FakeLockCompositor() { stop(); }
bool FakeLockCompositor::start(const std::string &socketName) {
  auto &self = *m_impl;
  self.display = wl_display_create();
  if (!self.display) return false;
  if (wl_display_add_socket(self.display, socketName.c_str()) != 0 || wl_display_init_shm(self.display) != 0) return false;
  wl_global_create(self.display, &wl_compositor_interface, 4, m_impl.get(),
      [](wl_client *client, void *data, uint32_t version, uint32_t id) {
        wl_resource *r = wl_resource_create(client, &wl_compositor_interface, static_cast<int>(version), id);
        if (r) wl_resource_set_implementation(r, &compositorImpl, data, nullptr);
      });
  wl_global_create(self.display, &wl_output_interface, 3, m_impl.get(),
      [](wl_client *client, void *data, uint32_t version, uint32_t id) {
        wl_resource *r = wl_resource_create(client, &wl_output_interface, static_cast<int>(version), id);
        if (!r) return;
        wl_resource_set_implementation(r, &outputImpl, data, nullptr);
        wl_output_send_geometry(r, 0, 0, 340, 270, WL_OUTPUT_SUBPIXEL_UNKNOWN, "Qinda", "Fake", WL_OUTPUT_TRANSFORM_NORMAL);
        wl_output_send_mode(r, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED, 1280, 1024, 60000);
        wl_output_send_scale(r, 1);
        wl_output_send_done(r);
      });
  wl_global_create(self.display, &ext_session_lock_manager_v1_interface, 1, m_impl.get(),
      [](wl_client *client, void *data, uint32_t version, uint32_t id) {
        wl_resource *r = wl_resource_create(client, &ext_session_lock_manager_v1_interface, static_cast<int>(version), id);
        if (r) wl_resource_set_implementation(r, &managerImpl, data, nullptr);
      });
  self.eventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (self.eventFd < 0) return false;
  self.wake = wl_event_loop_add_fd(wl_display_get_event_loop(self.display), self.eventFd, WL_EVENT_READABLE,
                                   &LockCompositorState::drain, m_impl.get());
  self.thread = std::thread([display = self.display] { wl_display_run(display); });
  return true;
}
void FakeLockCompositor::stop() {
  auto &self = *m_impl;
  if (!self.display) return;
  if (self.thread.joinable()) {
    self.run<int>([&self] { wl_display_terminate(self.display); return 0; });
    self.thread.join();
  }
  if (self.eventFd >= 0) { close(self.eventFd); self.eventFd = -1; }
  wl_display_destroy_clients(self.display);
  wl_display_destroy(self.display);
  self.display = nullptr;
}
std::uint32_t FakeLockCompositor::sendConfigure(std::uint32_t width, std::uint32_t height) {
  auto &self = *m_impl;
  return self.run<std::uint32_t>([&self, width, height] {
    if (!self.lockSurface) return std::uint32_t{0};
    self.lastSerial = wl_display_next_serial(self.display);
    ext_session_lock_surface_v1_send_configure(self.lockSurface, self.lastSerial, width, height);
    return self.lastSerial;
  });
}
void FakeLockCompositor::sendLocked() {
  auto &self = *m_impl;
  self.run<int>([&self] { if (self.lock) ext_session_lock_v1_send_locked(self.lock); return 0; });
}
void FakeLockCompositor::sendFinished() {
  auto &self = *m_impl;
  self.run<int>([&self] { if (self.lock) ext_session_lock_v1_send_finished(self.lock); return 0; });
}
std::vector<FakeLockCompositor::Observation> FakeLockCompositor::observations() const {
  std::lock_guard<std::mutex> guard(m_impl->mutex);
  return m_impl->seen;
}
std::size_t FakeLockCompositor::count(Observation::Kind kind) const {
  std::size_t total = 0;
  for (const auto &entry : observations()) if (entry.kind == kind) ++total;
  return total;
}
} // namespace QindaQt::LockProtocol::Testing
