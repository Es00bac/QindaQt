// SPDX-License-Identifier: GPL-3.0-or-later

// Lifetime of the impl modules the worker loads into its own PipeWire context
// - sends, endpoints, racks. Split from wireplumber_worker.cpp so the worker's
// core lifecycle and the module discipline read separately and each file
// stays inside its source-shape budget.

#include "wireplumber_worker_p.h"

#include <pipewire/pipewire.h>
#include <pipewire/impl-module.h>

#include <algorithm>
#include <utility>

namespace QindaQt::Audio
{

namespace {

struct ModuleHold {
    WirePlumberWorker *worker = nullptr;
    WirePlumberWorker::ModuleKind kind = WirePlumberWorker::ModuleKind::Send;
    std::string key;
    void *module = nullptr;
    spa_hook listener{};
};

void onModuleDestroy(void *data)
{
    auto *const hold = static_cast<ModuleHold *>(data);
    hold->worker->forgetModule(hold->kind, hold->key, hold->module);
    spa_hook_remove(&hold->listener);
    delete hold;
}

constexpr pw_impl_module_events kModuleEvents = {
    .version = PW_VERSION_IMPL_MODULE_EVENTS,
    .destroy = onModuleDestroy,
    .free = nullptr,
    .initialized = nullptr,
    .registered = nullptr,
};

} // namespace

void WirePlumberWorker::watchModule(const ModuleKind kind, const std::string &key,
                                    void *const module)
{
    auto *const hold = new ModuleHold{.worker = this,
                                      .kind = kind,
                                      .key = key,
                                      .module = module,
                                      .listener = {}};
    pw_impl_module_add_listener(static_cast<struct pw_impl_module *>(module),
                                &hold->listener, &kModuleEvents, hold);
}

void WirePlumberWorker::forgetModule(const ModuleKind kind, const std::string &key,
                                     void *const module)
{
    auto &map = kind == ModuleKind::Send ? m_routingModules
        : kind == ModuleKind::Chain    ? m_processingModules
                                       : m_busProcessingModules;
    const auto it = map.find(key);
    if (it != map.end() && it->second.module == module) {
        map.erase(it);
    }
    // A module queued for deferred destruction that PipeWire destroyed first
    // must not be destroyed again.
    std::erase(m_modulesPendingDestroy, module);
}

void WirePlumberWorker::destroyModuleLater(void *const module)
{
    if (module == nullptr) {
        return;
    }
    m_modulesPendingDestroy.push_back(module);
    if (m_moduleDestroySource != nullptr || m_context == nullptr) {
        return;
    }
    GSource *const source = g_idle_source_new();
    g_source_set_callback(source, dispatchModuleDestroys, this, nullptr);
    g_source_attach(source, m_context);
    m_moduleDestroySource = source;
}

gboolean WirePlumberWorker::dispatchModuleDestroys(gpointer data)
{
    auto *const self = static_cast<WirePlumberWorker *>(data);
    GSource *const source = std::exchange(self->m_moduleDestroySource, nullptr);
    self->flushPendingModuleDestroys();
    if (source != nullptr) {
        g_source_unref(source);
    }
    return G_SOURCE_REMOVE;
}

void WirePlumberWorker::flushPendingModuleDestroys()
{
    // Take the list first: destroying a module can re-enter through the
    // object manager and queue more.
    std::vector<void *> pending;
    pending.swap(m_modulesPendingDestroy);
    for (void *const module : pending) {
        pw_impl_module_destroy(static_cast<struct pw_impl_module *>(module));
    }
}

} // namespace QindaQt::Audio
