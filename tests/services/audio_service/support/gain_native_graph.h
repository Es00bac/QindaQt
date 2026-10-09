// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "console_routing_gain_p.h"
#include "console_send_admission_p.h"
#include "wireplumber_context_core_p.h"
#include "wireplumber_graph_p.h"

#include <QtCore/QList>
#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace QindaQt::Audio::Test
{
class PrivateGainRuntime;
struct NativeSendProps {
    std::array<float, 2> volumes{};
    bool muted = true;
};

// One connected, same-thread GLib owner over the fixture's private daemon.
// Modules/proxies die before Core, pending callbacks own only heap completion
// states, and manager signal callbacks are removed before this object dies.
class GainNativeGraph final
{
public:
    explicit GainNativeGraph(const PrivateGainRuntime &runtime);
    ~GainNativeGraph();
    GainNativeGraph(const GainNativeGraph &) = delete;
    GainNativeGraph &operator=(const GainNativeGraph &) = delete;
    [[nodiscard]] bool start();
    [[nodiscard]] bool wait(const std::function<bool()> &condition, int timeoutMs = 5000);
    void settle(int milliseconds = 150);
    [[nodiscard]] SendProducerIdentity producer(quint64 generation = 1) const;
    [[nodiscard]] QList<ObservedSendNode> nodes() const;
    [[nodiscard]] bool createSink(const QString &name);
    [[nodiscard]] pw_impl_module *load(const QByteArray &arguments,
                                      OwnedLoopbackLoadHooks hooks = {});
    [[nodiscard]] pw_impl_module *loadStandalone(const QByteArray &arguments);
    void destroy(pw_impl_module *module);
    [[nodiscard]] bool setGain(const ObservedSendNode &node, ConsoleSendAmplitude amplitude);
    [[nodiscard]] bool setById(quint32 id, ConsoleSendAmplitude amplitude);
    [[nodiscard]] std::optional<NativeSendProps> props(const ObservedSendNode &node) const;
    [[nodiscard]] WirePlumberGraph::BuildResult snapshot(
        const ConsoleSendAdmission *admission = nullptr) const;
    [[nodiscard]] WpCore *core() const { return m_core; }
    [[nodiscard]] WpObjectManager *manager() const { return m_manager; }
    [[nodiscard]] GMainContext *context() const { return m_context; }
    void disconnect();
    [[nodiscard]] const QString &failure() const { return m_failure; }

private:
    [[nodiscard]] pw_impl_module *holdModule(pw_impl_module *);
    struct ModuleHold;
    struct NodeHold;
    struct LoadState;
    static void loaded(GObject *, GAsyncResult *, gpointer);
    static void installed(WpObjectManager *, gpointer);
    static void moduleDestroyed(void *);
    static void nodeDestroyed(void *);
    const PrivateGainRuntime &m_runtime;
    GMainContext *m_context = nullptr;
    WpCore *m_core = nullptr;
    WpObjectManager *m_manager = nullptr;
    WpPlugin *m_mixer = nullptr;
    WpPlugin *m_defaults = nullptr;
    GCancellable *m_cancel = nullptr;
    QList<std::shared_ptr<LoadState>> m_loads;
    std::vector<std::unique_ptr<ModuleHold>> m_modules;
    std::vector<std::unique_ptr<NodeHold>> m_nodes;
    QString m_failure;
    bool m_installed = false;
    bool m_pushed = false;
};
} // namespace QindaQt::Audio::Test
