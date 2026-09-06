// SPDX-License-Identifier: GPL-3.0-or-later
//
// This is a private nested-session client qualification, not a compositor
// implementation. It deliberately uses the protocol generated from the
// installed Wayland XML so that native KWin ownership remains observable.

#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include <wayland-client.h>

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>

namespace {

using Clock = std::chrono::steady_clock;

struct Probe final {
    wl_display *display = nullptr;
    wl_registry *registry = nullptr;
    wl_compositor *compositor = nullptr;
    wl_shm *shm = nullptr;
    wl_seat *seat = nullptr;
    wl_pointer *pointer = nullptr;
    wl_surface *surface = nullptr;
    wl_buffer *buffer = nullptr;
    xdg_wm_base *xdgBase = nullptr;
    xdg_surface *xdgSurface = nullptr;
    xdg_toplevel *toplevel = nullptr;
    zwp_pointer_constraints_v1 *constraints = nullptr;
    zwp_relative_pointer_manager_v1 *relativeManager = nullptr;
    zwp_relative_pointer_v1 *relativePointer = nullptr;
    zwp_locked_pointer_v1 *lockedPointer = nullptr;
    zwp_confined_pointer_v1 *confinedPointer = nullptr;
    void *mappedBuffer = nullptr;
    std::size_t mappedLength = 0;
    int shmFd = -1;
    uint32_t seatGlobal = 0;
    uint32_t pointerEnterSerial = 0;
    bool seatHasPointer = false;
    bool surfaceMapped = false;
    bool lockRequested = false;
    bool lockReleaseRequested = false;
    bool locked = false;
    bool unlocked = false;
    bool confineRequested = false;
    bool confineReleaseRequested = false;
    bool confined = false;
    bool unconfined = false;
    bool relativeCreated = false;
    unsigned relativeMotionCount = 0;
    unsigned pointerMotionCount = 0;
    bool closed = false;
    std::string protocolError;
    std::string phase = "binding";
};

void rememberError(Probe &probe, const char *message)
{
    if (probe.protocolError.empty()) {
        probe.protocolError = message;
    }
}

void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface,
                    uint32_t version)
{
    auto &probe = *static_cast<Probe *>(data);
    if (std::strcmp(interface, "wl_compositor") == 0 && !probe.compositor) {
        probe.compositor = static_cast<wl_compositor *>(wl_registry_bind(
            registry, name, &wl_compositor_interface, std::min(version, 4u)));
    } else if (std::strcmp(interface, "wl_shm") == 0 && !probe.shm) {
        probe.shm = static_cast<wl_shm *>(
            wl_registry_bind(registry, name, &wl_shm_interface, std::min(version, 1u)));
    } else if (std::strcmp(interface, "wl_seat") == 0 && !probe.seat) {
        probe.seatGlobal = name;
        probe.seat = static_cast<wl_seat *>(
            wl_registry_bind(registry, name, &wl_seat_interface, std::min(version, 5u)));
    } else if (std::strcmp(interface, "xdg_wm_base") == 0 && !probe.xdgBase) {
        probe.xdgBase = static_cast<xdg_wm_base *>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, std::min(version, 6u)));
    } else if (std::strcmp(interface, "zwp_pointer_constraints_v1") == 0 && !probe.constraints) {
        probe.constraints = static_cast<zwp_pointer_constraints_v1 *>(wl_registry_bind(
            registry, name, &zwp_pointer_constraints_v1_interface, std::min(version, 1u)));
    } else if (std::strcmp(interface, "zwp_relative_pointer_manager_v1") == 0 &&
               !probe.relativeManager) {
        probe.relativeManager = static_cast<zwp_relative_pointer_manager_v1 *>(wl_registry_bind(
            registry, name, &zwp_relative_pointer_manager_v1_interface, std::min(version, 1u)));
    }
}

void registryGlobalRemove(void *, wl_registry *, uint32_t)
{
}

const wl_registry_listener registryListener = {registryGlobal, registryGlobalRemove};

void seatCapabilities(void *data, wl_seat *seat, uint32_t capabilities)
{
    auto &probe = *static_cast<Probe *>(data);
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) != 0 && !probe.pointer) {
        probe.pointer = wl_seat_get_pointer(seat);
        probe.seatHasPointer = probe.pointer != nullptr;
    }
}

void seatName(void *, wl_seat *, const char *)
{
}

const wl_seat_listener seatListener = {seatCapabilities, seatName};

void pointerEnter(void *data, wl_pointer *, uint32_t serial, wl_surface *surface, wl_fixed_t,
                  wl_fixed_t)
{
    auto &probe = *static_cast<Probe *>(data);
    if (surface == probe.surface) {
        probe.pointerEnterSerial = serial;
    }
}

void pointerLeave(void *, wl_pointer *, uint32_t, wl_surface *)
{
}

void pointerMotion(void *data, wl_pointer *, uint32_t, wl_fixed_t, wl_fixed_t)
{
    ++static_cast<Probe *>(data)->pointerMotionCount;
}

void pointerButton(void *, wl_pointer *, uint32_t, uint32_t, uint32_t, uint32_t)
{
}

void pointerAxis(void *, wl_pointer *, uint32_t, uint32_t, wl_fixed_t)
{
}

void pointerFrame(void *, wl_pointer *)
{
}

void pointerAxisSource(void *, wl_pointer *, uint32_t)
{
}

void pointerAxisStop(void *, wl_pointer *, uint32_t, uint32_t)
{
}

void pointerAxisDiscrete(void *, wl_pointer *, uint32_t, int32_t)
{
}

const wl_pointer_listener pointerListener = {pointerEnter, pointerLeave, pointerMotion,
                                             pointerButton, pointerAxis, pointerFrame,
                                             pointerAxisSource, pointerAxisStop,
                                             pointerAxisDiscrete, nullptr, nullptr};

void relativeMotion(void *data, zwp_relative_pointer_v1 *, uint32_t, uint32_t, wl_fixed_t,
                    wl_fixed_t, wl_fixed_t, wl_fixed_t)
{
    auto &probe = *static_cast<Probe *>(data);
    // A relative event before the lock is established only proves that the
    // manager exists.  The qualification must observe one while the lock is
    // active, otherwise a pre-lock pointer move can make the gate pass.
    if (probe.locked && !probe.unlocked) {
        ++probe.relativeMotionCount;
    }
}

const zwp_relative_pointer_v1_listener relativeListener = {relativeMotion};

void locked(void *data, zwp_locked_pointer_v1 *)
{
    static_cast<Probe *>(data)->locked = true;
}

void releaseLockedPointer(Probe &probe)
{
    if (probe.lockedPointer) {
        zwp_locked_pointer_v1_destroy(probe.lockedPointer);
        probe.lockedPointer = nullptr;
    }
    probe.lockReleaseRequested = true;
}

void unlocked(void *data, zwp_locked_pointer_v1 *)
{
    auto &probe = *static_cast<Probe *>(data);
    probe.unlocked = true;
    // KWin may withdraw the constraint in the same dispatch batch that
    // reports activation.  Both this callback and the main loop use the
    // idempotent release helper so that one event cannot double-destroy the
    // protocol proxy.
    releaseLockedPointer(probe);
}

const zwp_locked_pointer_v1_listener lockedListener = {locked, unlocked};

void confined(void *data, zwp_confined_pointer_v1 *)
{
    static_cast<Probe *>(data)->confined = true;
}

void releaseConfinedPointer(Probe &probe)
{
    if (probe.confinedPointer) {
        zwp_confined_pointer_v1_destroy(probe.confinedPointer);
        probe.confinedPointer = nullptr;
    }
    probe.confineReleaseRequested = true;
}

void unconfined(void *data, zwp_confined_pointer_v1 *)
{
    auto &probe = *static_cast<Probe *>(data);
    probe.unconfined = true;
    releaseConfinedPointer(probe);
}

const zwp_confined_pointer_v1_listener confinedListener = {confined, unconfined};

void xdgPing(void *, xdg_wm_base *base, uint32_t serial)
{
    xdg_wm_base_pong(base, serial);
}

const xdg_wm_base_listener xdgBaseListener = {xdgPing};

void surfaceConfigure(void *data, xdg_surface *surface, uint32_t serial)
{
    auto &probe = *static_cast<Probe *>(data);
    xdg_surface_ack_configure(surface, serial);
    if (!probe.surfaceMapped && probe.buffer) {
        wl_surface_attach(probe.surface, probe.buffer, 0, 0);
        // `damage` is available on every wl_surface version used by this
        // probe; avoid requiring the version-4 damage_buffer request.
        wl_surface_damage(probe.surface, 0, 0, 320, 240);
        wl_surface_commit(probe.surface);
        probe.surfaceMapped = true;
    }
}

const xdg_surface_listener xdgSurfaceListener = {surfaceConfigure};

void toplevelConfigure(void *, xdg_toplevel *, int32_t, int32_t, wl_array *)
{
}

void toplevelClose(void *data, xdg_toplevel *)
{
    static_cast<Probe *>(data)->closed = true;
}

void toplevelConfigureBounds(void *, xdg_toplevel *, int32_t, int32_t)
{
}

void toplevelWmCapabilities(void *, xdg_toplevel *, wl_array *)
{
}

const xdg_toplevel_listener toplevelListener = {toplevelConfigure, toplevelClose,
                                                toplevelConfigureBounds, toplevelWmCapabilities};

void bufferRelease(void *, wl_buffer *buffer)
{
    // Cleanup owns the proxy lifetime. Keeping it alive here also makes the
    // later surface teardown deterministic when the compositor releases the
    // attached buffer before the probe exits.
    static_cast<void>(buffer);
}

const wl_buffer_listener bufferListener = {bufferRelease};

bool createBuffer(Probe &probe)
{
    char name[64];
    std::snprintf(name, sizeof(name), "/qindaqt-pointer-probe-%ld", static_cast<long>(getpid()));
    probe.shmFd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (probe.shmFd < 0) {
        probe.protocolError = "shm_open: " + std::string(std::strerror(errno));
        return false;
    }
    shm_unlink(name);
    constexpr std::size_t stride = 320 * 4;
    probe.mappedLength = stride * 240;
    if (ftruncate(probe.shmFd, static_cast<off_t>(probe.mappedLength)) != 0) {
        rememberError(probe, "ftruncate failed");
        return false;
    }
    probe.mappedBuffer = mmap(nullptr, probe.mappedLength, PROT_READ | PROT_WRITE, MAP_SHARED,
                              probe.shmFd, 0);
    if (probe.mappedBuffer == MAP_FAILED) {
        probe.mappedBuffer = nullptr;
        rememberError(probe, "mmap failed");
        return false;
    }
    auto *pixels = static_cast<uint32_t *>(probe.mappedBuffer);
    for (std::size_t i = 0; i < probe.mappedLength / sizeof(uint32_t); ++i) {
        pixels[i] = 0xff26333f;
    }
    wl_shm_pool *pool = wl_shm_create_pool(probe.shm, probe.shmFd,
                                            static_cast<int32_t>(probe.mappedLength));
    if (!pool) {
        rememberError(probe, "wl_shm_create_pool failed");
        return false;
    }
    probe.buffer = wl_shm_pool_create_buffer(pool, 0, 320, 240, static_cast<int32_t>(stride),
                                             WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    if (!probe.buffer) {
        rememberError(probe, "wl_shm_pool_create_buffer failed");
        return false;
    }
    wl_buffer_add_listener(probe.buffer, &bufferListener, &probe);
    return true;
}

bool createSurface(Probe &probe)
{
    probe.surface = wl_compositor_create_surface(probe.compositor);
    if (!probe.surface || !probe.xdgBase || !probe.shm) {
        rememberError(probe, "required core globals missing");
        return false;
    }
    if (!createBuffer(probe)) {
        return false;
    }
    xdg_wm_base_add_listener(probe.xdgBase, &xdgBaseListener, &probe);
    probe.xdgSurface = xdg_wm_base_get_xdg_surface(probe.xdgBase, probe.surface);
    probe.toplevel = xdg_surface_get_toplevel(probe.xdgSurface);
    if (!probe.xdgSurface || !probe.toplevel) {
        rememberError(probe, "xdg surface creation failed");
        return false;
    }
    xdg_surface_add_listener(probe.xdgSurface, &xdgSurfaceListener, &probe);
    xdg_toplevel_add_listener(probe.toplevel, &toplevelListener, &probe);
    xdg_toplevel_set_title(probe.toplevel, "QindaQt pointer constraints probe");
    xdg_toplevel_set_app_id(probe.toplevel, "org.qindaqt.pointer-probe");
    wl_surface_commit(probe.surface);
    if (wl_display_roundtrip(probe.display) < 0 || !probe.surfaceMapped) {
        rememberError(probe, "surface did not receive an initial configure");
        return false;
    }
    return true;
}

bool makeRelativePointer(Probe &probe)
{
    probe.relativePointer = zwp_relative_pointer_manager_v1_get_relative_pointer(
        probe.relativeManager, probe.pointer);
    if (!probe.relativePointer) {
        rememberError(probe, "relative pointer creation failed");
        return false;
    }
    zwp_relative_pointer_v1_add_listener(probe.relativePointer, &relativeListener, &probe);
    probe.relativeCreated = true;
    return true;
}

bool beginLock(Probe &probe)
{
    probe.phase = "locking";
    probe.lockedPointer = zwp_pointer_constraints_v1_lock_pointer(
        probe.constraints, probe.surface, probe.pointer, nullptr,
        ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);
    if (!probe.lockedPointer) {
        rememberError(probe, "lock_pointer request failed");
        return false;
    }
    zwp_locked_pointer_v1_add_listener(probe.lockedPointer, &lockedListener, &probe);
    probe.lockRequested = true;
    wl_display_flush(probe.display);
    return true;
}

bool beginConfine(Probe &probe)
{
    probe.phase = "confining";
    probe.confinedPointer = zwp_pointer_constraints_v1_confine_pointer(
        probe.constraints, probe.surface, probe.pointer, nullptr,
        ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_ONESHOT);
    if (!probe.confinedPointer) {
        rememberError(probe, "confine_pointer request failed");
        return false;
    }
    zwp_confined_pointer_v1_add_listener(probe.confinedPointer, &confinedListener, &probe);
    probe.confineRequested = true;
    wl_display_flush(probe.display);
    return true;
}

bool waitFor(Probe &probe, const std::function<bool()> &condition, int timeoutMs)
{
    const auto deadline = Clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!condition() && !probe.closed && probe.protocolError.empty() && Clock::now() < deadline) {
        if (wl_display_flush(probe.display) < 0 && errno != EAGAIN) {
            rememberError(probe, "wl_display_flush failed");
            break;
        }
        pollfd descriptor{wl_display_get_fd(probe.display), POLLIN, 0};
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   deadline - Clock::now())
                                   .count();
        const int waitMs = static_cast<int>(std::clamp<long long>(remaining, 1, 100));
        const int result = poll(&descriptor, 1, waitMs);
        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result < 0) {
            rememberError(probe, "poll failed");
            break;
        }
        if (result > 0 && wl_display_dispatch(probe.display) < 0) {
            rememberError(probe, "wl_display_dispatch failed");
            break;
        }
    }
    return condition();
}

void destroyProbe(Probe &probe)
{
    if (probe.confinedPointer) {
        zwp_confined_pointer_v1_destroy(probe.confinedPointer);
    }
    if (probe.lockedPointer) {
        zwp_locked_pointer_v1_destroy(probe.lockedPointer);
    }
    if (probe.relativePointer) {
        zwp_relative_pointer_v1_destroy(probe.relativePointer);
    }
    if (probe.constraints) {
        zwp_pointer_constraints_v1_destroy(probe.constraints);
    }
    if (probe.relativeManager) {
        zwp_relative_pointer_manager_v1_destroy(probe.relativeManager);
    }
    if (probe.toplevel) {
        xdg_toplevel_destroy(probe.toplevel);
    }
    if (probe.xdgSurface) {
        xdg_surface_destroy(probe.xdgSurface);
    }
    if (probe.surface) {
        wl_surface_destroy(probe.surface);
    }
    if (probe.buffer) {
        wl_buffer_destroy(probe.buffer);
    }
    if (probe.shm) {
        wl_shm_destroy(probe.shm);
    }
    if (probe.seat) {
        if (probe.pointer) {
            wl_pointer_destroy(probe.pointer);
        }
        wl_seat_destroy(probe.seat);
    }
    if (probe.compositor) {
        wl_compositor_destroy(probe.compositor);
    }
    if (probe.xdgBase) {
        xdg_wm_base_destroy(probe.xdgBase);
    }
    if (probe.registry) {
        wl_registry_destroy(probe.registry);
    }
    if (probe.display) {
        wl_display_disconnect(probe.display);
    }
    if (probe.mappedBuffer) {
        munmap(probe.mappedBuffer, probe.mappedLength);
    }
    if (probe.shmFd >= 0) {
        close(probe.shmFd);
    }
}

void printResult(const Probe &probe)
{
    std::printf(
        "QINDAQT_POINTER_PROBE={\"globals\":{\"compositor\":%s,\"seat\":%s,\"xdgShell\":%s,\"pointerConstraints\":%s,\"relativePointer\":%s},\"surfaceMapped\":%s,\"pointerReady\":%s,\"relativeObjectCreated\":%s,\"lockRequested\":%s,\"locked\":%s,\"relativeMotionCount\":%u,\"lockReleaseRequested\":%s,\"unlocked\":%s,\"confineRequested\":%s,\"confined\":%s,\"confineReleaseRequested\":%s,\"unconfined\":%s,\"pointerMotionCount\":%u,\"phase\":\"%s\",\"error\":\"%s\"}\n",
        probe.compositor ? "true" : "false", probe.seat ? "true" : "false",
        probe.xdgBase ? "true" : "false", probe.constraints ? "true" : "false",
        probe.relativeManager ? "true" : "false", probe.surfaceMapped ? "true" : "false",
        probe.seatHasPointer ? "true" : "false", probe.relativeCreated ? "true" : "false",
        probe.lockRequested ? "true" : "false", probe.locked ? "true" : "false",
        probe.relativeMotionCount, probe.lockReleaseRequested ? "true" : "false",
        probe.unlocked ? "true" : "false", probe.confineRequested ? "true" : "false",
        probe.confined ? "true" : "false", probe.confineReleaseRequested ? "true" : "false",
        probe.unconfined ? "true" : "false", probe.pointerMotionCount, probe.phase.c_str(),
        probe.protocolError.c_str());
}

} // namespace

int main(int argc, char **argv)
{
    const int timeoutMs = argc > 1 ? std::max(100, std::atoi(argv[1])) : 3000;
    Probe probe;
    probe.display = wl_display_connect(nullptr);
    if (!probe.display) {
        std::fprintf(stderr, "QindaQt pointer probe requires WAYLAND_DISPLAY\n");
        return 77;
    }
    probe.registry = wl_display_get_registry(probe.display);
    wl_registry_add_listener(probe.registry, &registryListener, &probe);
    if (wl_display_roundtrip(probe.display) < 0 || !probe.compositor || !probe.shm || !probe.seat ||
        !probe.xdgBase || !probe.constraints || !probe.relativeManager) {
        rememberError(probe, "required Wayland globals are unavailable");
        printResult(probe);
        destroyProbe(probe);
        return 1;
    }
    wl_seat_add_listener(probe.seat, &seatListener, &probe);
    if (wl_display_roundtrip(probe.display) < 0 || !probe.pointer) {
        rememberError(probe, "seat did not expose a pointer");
        printResult(probe);
        destroyProbe(probe);
        return 1;
    }
    wl_pointer_add_listener(probe.pointer, &pointerListener, &probe);
    if (!createSurface(probe) || !makeRelativePointer(probe) || !beginLock(probe)) {
        printResult(probe);
        destroyProbe(probe);
        return 1;
    }

    // The private session runner moves a synthetic pointer onto this mapped
    // surface and then sends a second motion while the lock is active. The
    // probe itself never touches host input devices.
    waitFor(probe, [&probe] { return probe.locked; }, timeoutMs);
    if (probe.locked) {
        // Keep the lock alive until a relative event has arrived after the
        // activation callback.  This separates actual locked motion from
        // events delivered while the pointer merely existed.
        waitFor(probe, [&probe] { return probe.relativeMotionCount > 0; }, timeoutMs);
        // Destroy is the protocol-defined client release operation. An
        // `unlocked` event may have already withdrawn the proxy; the helper
        // remains safe in either ordering.
        releaseLockedPointer(probe);
        wl_display_roundtrip(probe.display);
    }
    if (probe.lockReleaseRequested) {
        beginConfine(probe);
        waitFor(probe, [&probe] { return probe.confined; }, timeoutMs);
        if (probe.confined) {
            // Destroying a oneshot constraint after activation is the
            // protocol-defined release operation. If KWin deactivates it
            // independently, the unconfined event is recorded as well.
            releaseConfinedPointer(probe);
            wl_display_roundtrip(probe.display);
        }
    }
    probe.phase = probe.confineReleaseRequested ? "complete" : "incomplete";
    printResult(probe);
    const bool passed = probe.surfaceMapped && probe.relativeCreated && probe.lockRequested &&
                        probe.locked && probe.lockReleaseRequested && probe.relativeMotionCount > 0 &&
                        probe.confineRequested && probe.confined && probe.confineReleaseRequested &&
                        probe.protocolError.empty();
    destroyProbe(probe);
    return passed ? 0 : 1;
}
