// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <pipewire/pipewire.h>
#include <pipewire/impl-module.h>
#include <wp/wp.h>
#include <QtCore/QByteArray>
#include <functional>

namespace QindaQt::Audio
{

// Negative-only injection: production uses the real public context API.
// Overridden functions must have those exact semantics and outlive the scope.
struct ContextObjectAccess {
    decltype(&pw_context_get_object) get = &pw_context_get_object;
    decltype(&pw_context_set_object) set = &pw_context_set_object;
};

// ADR0361 prospective contract: this same-thread scope borrows a connected
// Core ONLY across one synchronous fixed module load. It never drives an event
// loop, disconnects Core, or replaces/removes another owner's slot. Nested
// loads (including a same-pointer occupant) fail closed. Caller keeps WpCore
// alive and destroys returned modules before the owning context is released.
class ScopedContextCore final
{
public:
    explicit ScopedContextCore(WpCore *core, ContextObjectAccess access = {});
    ~ScopedContextCore();
    ScopedContextCore(const ScopedContextCore &) = delete;
    ScopedContextCore &operator=(const ScopedContextCore &) = delete;

    [[nodiscard]] bool admitted() const noexcept;
    // Idempotent; removes only the exact entry this scope successfully inserted.
    // A raw proxy reference protects its memory until the entry is gone. It
    // does not keep a disconnected Core operational or authorize a late load.
    void release() noexcept;

private:
    static void onCoreDestroy(void *data);
    void revokeSlot() noexcept;

    ContextObjectAccess m_access;
    WpCore *m_wpCore = nullptr;
    pw_context *m_context = nullptr;
    pw_core *m_rawCore = nullptr;
    pw_proxy *m_proxy = nullptr;
    spa_hook m_listener{};
    bool m_inserted = false;
    bool m_live = false;
    bool m_listening = false;
};

// A required image gate plus subtractive-only load-time lifetime negatives.
// Production worker uses this only behind the release-owned image policy.
struct OwnedLoopbackLoadHooks {
    // Required borrowed read-only/non-reentrant image gate, same thread and
    // synchronous call lifetime. Missing proof refuses before slot insertion.
    // Called again after subtractive beforeLoad hooks, immediately before the
    // fixed public factory; it must not dispatch or change Core/loader state.
    std::function<bool()> admitModule = {};
    std::function<void()> beforeLoad;
};
// Returned module is caller-owned and must die before its context. This fixed
// loopback-only boundary does no event-loop dispatch and removes its context
// slot before returning on every path.
[[nodiscard]] pw_impl_module *loadOwnedLoopback(
    WpCore *core, const QByteArray &arguments, OwnedLoopbackLoadHooks hooks = {});

} // namespace QindaQt::Audio
