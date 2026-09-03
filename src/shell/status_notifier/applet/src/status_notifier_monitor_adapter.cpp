// SPDX-License-Identifier: LGPL-3.0-or-later

#include "qindaqt/shell/status_notifier/applet/status_notifier_monitor_adapter.h"

#include <qindaqt/shell/status_notifier/status_notifier_presentation.h>

namespace QindaQt::StatusNotifierApplet {

using namespace QindaQt::StatusNotifier;

// Every override forwards verbatim to the registry and emits the adapter's
// changed() only after an accepted mutation, so QML reprojects exactly when
// presented state could have moved. A refused (stale/hostile) event never
// notifies.
class StatusNotifierMonitorAdapter::ForwardingSink final : public StatusNotifierEventSink {
public:
    ForwardingSink(StatusNotifierRegistry &registry, StatusNotifierMonitorAdapter &adapter)
        : m_registry(registry)
        , m_adapter(adapter)
    {
    }

    quint64 beginWatcherEpoch() override
    {
        const quint64 epoch = m_registry.beginWatcherEpoch();
        if (epoch != 0) {
            Q_EMIT m_adapter.changed();
        }
        return epoch;
    }

    RegistryOutcome markInitialPopulationComplete(quint64 epoch) override
    {
        const RegistryOutcome outcome = m_registry.markInitialPopulationComplete(epoch);
        if (outcome.accepted()) {
            Q_EMIT m_adapter.changed();
        }
        return outcome;
    }

    quint64 beginOwnerGeneration(quint64 epoch, const QString &uniqueName) override
    {
        const quint64 generation = m_registry.beginOwnerGeneration(epoch, uniqueName);
        if (generation != 0) {
            Q_EMIT m_adapter.changed();
        }
        return generation;
    }

    RegistryOutcome ownerLost(quint64 epoch,
                              const QString &uniqueName,
                              quint64 expectedGeneration) override
    {
        return forward(m_registry.ownerLost(epoch, uniqueName, expectedGeneration));
    }

    RegistryOutcome registerItem(quint64 epoch,
                                 const OwnerKey &key,
                                 const ItemDescriptor &descriptor) override
    {
        return forward(m_registry.registerItem(epoch, key, descriptor));
    }

    RegistryOutcome removeItem(quint64 epoch, const OwnerKey &key) override
    {
        return forward(m_registry.removeItem(epoch, key));
    }

    RegistryOutcome removeAllForOwner(quint64 epoch,
                                      const QString &uniqueName,
                                      quint64 generation) override
    {
        return forward(m_registry.removeAllForOwner(epoch, uniqueName, generation));
    }

private:
    RegistryOutcome forward(const RegistryOutcome &outcome)
    {
        if (outcome.accepted()) {
            Q_EMIT m_adapter.changed();
        }
        return outcome;
    }

    StatusNotifierRegistry &m_registry;
    StatusNotifierMonitorAdapter &m_adapter;
};

StatusNotifierMonitorAdapter::StatusNotifierMonitorAdapter(QDBusConnection connection,
                                                           QStringList iconThemeRoots,
                                                           int fetchTimeoutMs,
                                                           QObject *parent)
    : StatusNotifierSourceInterface(parent)
    , m_registry()
    , m_sink(std::make_unique<ForwardingSink>(m_registry, *this))
    , m_monitor(connection, m_registry, fetchTimeoutMs)
    , m_renderer(std::move(iconThemeRoots))
{
    // Watcher (re)appearance and loss change presentation (Loading/Degraded)
    // without touching the registry, so liveness transitions notify too.
    connect(&m_monitor, &StatusNotifierItemMonitor::watcherLiveChanged,
            this, &StatusNotifierMonitorAdapter::changed);
}

StatusNotifierMonitorAdapter::~StatusNotifierMonitorAdapter()
{
    stop();
}

void StatusNotifierMonitorAdapter::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    m_monitor.attach(m_sink.get());
}

void StatusNotifierMonitorAdapter::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    m_monitor.detach();
}

bool StatusNotifierMonitorAdapter::isRunning() const
{
    return m_running && m_monitor.isAttached();
}

TrayPresentation StatusNotifierMonitorAdapter::presentation() const
{
    return projectPresentation(m_registry, { .transportLive = m_monitor.isWatcherLive() });
}

QList<ItemDescriptor> StatusNotifierMonitorAdapter::itemDescriptors() const
{
    return m_registry.items();
}

QImage StatusNotifierMonitorAdapter::renderIcon(const OwnerKey &key, int size) const
{
    // AGENT-CONTRACT: never null. An unknown/stale key renders the shared
    // deterministic placeholder, and the S1 renderer itself falls back to the
    // same placeholder for missing or hostile payloads.
    const std::optional<ItemDescriptor> descriptor = m_registry.find(key);
    if (!descriptor.has_value()) {
        return StatusNotifierIconRenderer::fallbackIcon(size);
    }
    return m_renderer.render(descriptor->icon, size);
}

quint64 StatusNotifierMonitorAdapter::currentGeneration(const QString &uniqueName) const
{
    return m_registry.currentGeneration(uniqueName);
}

RegistryOutcome StatusNotifierMonitorAdapter::activate(const OwnerKey &target, int x, int y)
{
    return m_monitor.requestActivate(target, x, y);
}

RegistryOutcome StatusNotifierMonitorAdapter::secondaryActivate(const OwnerKey &target,
                                                                int x,
                                                                int y)
{
    return m_monitor.requestSecondaryActivate(target, x, y);
}

RegistryOutcome StatusNotifierMonitorAdapter::contextMenu(const OwnerKey &target, int x, int y)
{
    return m_monitor.requestContextMenu(target, x, y);
}

} // namespace QindaQt::StatusNotifierApplet
