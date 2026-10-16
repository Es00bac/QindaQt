// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace QindaQt::LockProtocol::Testing {
struct LockCompositorState; // Owns the libwayland-server objects; private to the .cpp.
// A just-enough Wayland compositor for the Qt session-lock shell integration:
// wl_compositor, wl_shm, one wl_output and ext_session_lock_manager_v1, run on
// its own thread by libwayland-server. It enforces no policy; it only records
// what the real client sends (lock surface requests, ack_configure serials,
// committed buffer sizes) and sends the configure/locked events a test scripts.
// All commands are thread-safe and execute on the compositor thread.
class FakeLockCompositor {
public:
  struct Observation {
    enum class Kind { LockRequested, LockSurfaceRequested, LockSurfaceDestroyed, AckConfigure, Commit, UnlockRequested };
    Kind kind;
    std::uint32_t serial = 0;  // AckConfigure
    int width = 0, height = 0; // Commit: size of the attached buffer
  };
  FakeLockCompositor();
  ~FakeLockCompositor();
  // Listens on $XDG_RUNTIME_DIR/<socketName>; the caller points WAYLAND_DISPLAY at it.
  bool start(const std::string &socketName);
  void stop();
  // Sends ext_session_lock_surface_v1.configure to the lock surface and returns its serial
  // (0 when no lock surface exists yet).
  std::uint32_t sendConfigure(std::uint32_t width, std::uint32_t height);
  void sendLocked();
  // The compositor decides the lock will never be used (ext_session_lock_v1.finished).
  void sendFinished();
  std::vector<Observation> observations() const;
  std::size_t count(Observation::Kind kind) const;
private:
  std::unique_ptr<LockCompositorState> m_impl;
};
} // namespace QindaQt::LockProtocol::Testing
