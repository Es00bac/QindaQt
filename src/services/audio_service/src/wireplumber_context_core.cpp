// SPDX-License-Identifier: GPL-3.0-or-later
#include "wireplumber_context_core_p.h"
#include <pipewire/impl-module.h>

namespace QindaQt::Audio
{
ScopedContextCore::ScopedContextCore(WpCore *core, const ContextObjectAccess access)
    : m_access(access), m_wpCore(core)
{
    if (core == nullptr || access.get == nullptr || access.set == nullptr
        || !wp_core_is_connected(core)
        || !g_main_context_is_owner(wp_core_get_g_main_context(core))) {
        return;
    }
    m_context = wp_core_get_pw_context(core);
    m_rawCore = wp_core_get_pw_core(core);
    if (m_context == nullptr || m_rawCore == nullptr
        || access.get(m_context, PW_TYPE_INTERFACE_Core) != nullptr) {
        m_context = nullptr;
        m_rawCore = nullptr;
        return;
    }
    m_proxy = reinterpret_cast<pw_proxy *>(m_rawCore);
    // AGENT-GUARD: WP0.5.15 disconnected is emitted from proxy destroy AFTER
    // PipeWire tears down Core internals. Keep the raw proxy memory referenced
    // and remove its slot in the raw destroy event, before proxy's final free.
    // This reference is lifetime protection, never connected-state authority.
    pw_proxy_ref(m_proxy);
    static const pw_proxy_events events = [] {
        pw_proxy_events value{};
        value.version = PW_VERSION_PROXY_EVENTS;
        value.destroy = &ScopedContextCore::onCoreDestroy;
        return value;
    }();
    pw_proxy_add_listener(m_proxy, &m_listener, &events, this);
    m_listening = true;
    if (access.set(m_context, PW_TYPE_INTERFACE_Core, m_rawCore) < 0) {
        release();
        return;
    }
    m_inserted = true;
    m_live = true;
}

ScopedContextCore::~ScopedContextCore()
{
    release();
}

bool ScopedContextCore::admitted() const noexcept
{
    return m_live && m_inserted && m_wpCore != nullptr
        && wp_core_is_connected(m_wpCore)
        && wp_core_get_pw_core(m_wpCore) == m_rawCore
        && m_access.get(m_context, PW_TYPE_INTERFACE_Core) == m_rawCore;
}

void ScopedContextCore::revokeSlot() noexcept
{
    m_live = false;
    if (m_inserted) {
        // A changed slot belongs to someone else. The scope may never remove
        // that new occupant, even on an error/recursive destruction path.
        if (m_access.get(m_context, PW_TYPE_INTERFACE_Core) == m_rawCore) {
            // Real context API removal does not allocate and returns0.
            (void)m_access.set(m_context, PW_TYPE_INTERFACE_Core, nullptr);
        }
        m_inserted = false;
    }
}

void ScopedContextCore::onCoreDestroy(void *data)
{
    static_cast<ScopedContextCore *>(data)->revokeSlot();
}

void ScopedContextCore::release() noexcept
{
    revokeSlot();
    if (m_listening) {
        spa_hook_remove(&m_listener);
        m_listening = false;
    }
    if (m_proxy != nullptr) {
        pw_proxy_unref(m_proxy);
        m_proxy = nullptr;
    }
    m_rawCore = nullptr;
    m_context = nullptr;
    m_wpCore = nullptr;
}

pw_impl_module *loadOwnedLoopback(WpCore *core, const QByteArray &arguments,
                                  OwnedLoopbackLoadHooks hooks)
{
    if (arguments.isEmpty() || !hooks.admitModule || !hooks.admitModule()) {
        return nullptr;
    }
    ScopedContextCore scope(core);
    if (!scope.admitted()) {
        return nullptr;
    }
    if (hooks.beforeLoad) {
        hooks.beforeLoad();
    }
    if (!scope.admitted() || !hooks.admitModule()) {
        return nullptr;
    }
    auto *module = pw_context_load_module(wp_core_get_pw_context(core),
        "libpipewire-module-loopback", arguments.constData(), nullptr);
    if (!scope.admitted()) {
        if (module != nullptr) {
            pw_impl_module_destroy(module);
        }
        return nullptr;
    }
    scope.release();
    return module;
}

} // namespace QindaQt::Audio
