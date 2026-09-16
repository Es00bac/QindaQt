// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_graph_p.h"
#include "wireplumber_worker_p.h"

#include <pipewire/pipewire.h>

#include <utility>

namespace QindaQt::Audio
{
namespace {

// 20 Hz. Fast enough that a meter looks live and a transient is caught, slow
// enough that the bus carries a few small signals a second rather than a
// stream. The reference console updates at a comparable rate.
constexpr guint kMeterPollIntervalMs = 50;

} // namespace

void WirePlumberWorker::applyMetering(QList<BackendMeterTarget> targets)
{
    invoke([this, targets = std::move(targets)] { applyMeteringOnWorker(targets); });
}

void WirePlumberWorker::applyMeteringOnWorker(const QList<BackendMeterTarget> &targets)
{
    m_declaredMetering = targets;
    if (m_core == nullptr || m_manager == nullptr) {
        return;
    }
    struct pw_context *const context = wp_core_get_pw_context(m_core);
    if (context == nullptr) {
        return;
    }

    QList<MeterBank::Target> resolved;
    resolved.reserve(targets.size());
    for (const BackendMeterTarget &target : targets) {
        // A target whose device is not in the graph right now is simply not
        // metered; it comes back on the next rebuild without the console
        // having to restate anything.
        bool captureSink = target.captureSink;
        // A strip with a running rack is metered after it (ADR-0179); a bus
        // never has one and reads its device as before.
        const QString nodeName = target.consoleId.startsWith(QLatin1String("strip."))
            ? stripReadNode(target.consoleId, target.device, &captureSink)
            : nodeNameForHandle(target.device);
        if (target.consoleId.isEmpty() || nodeName.isEmpty()) {
            continue;
        }
        resolved.append(MeterBank::Target{.consoleId = target.consoleId,
                                          .nodeName = nodeName,
                                          .captureSink = captureSink,
                                          .passive = target.passive});
    }
    m_meters.configure(context, resolved);
    if (resolved.isEmpty()) {
        stopMeterPolling();
        if (!m_lastLevels.isEmpty()) {
            // Metering stopped: say so once, so a console clears its meters
            // instead of freezing on the last reading it saw.
            m_lastLevels.clear();
            if (m_levelsCallback) {
                m_levelsCallback({});
            }
        }
        return;
    }
    startMeterPolling();
}

void WirePlumberWorker::startMeterPolling()
{
    if (m_meterSource != nullptr || m_context == nullptr) {
        return;
    }
    GSource *const source = g_timeout_source_new(kMeterPollIntervalMs);
    g_source_set_callback(source, dispatchMeterPoll, this, nullptr);
    g_source_attach(source, m_context);
    // The worker retains the source and destroys it explicitly: a poll that
    // outlived the context would read streams that no longer exist.
    m_meterSource = source;
}

void WirePlumberWorker::stopMeterPolling()
{
    GSource *const source = std::exchange(m_meterSource, nullptr);
    if (source == nullptr) {
        return;
    }
    g_source_destroy(source);
    g_source_unref(source);
}

gboolean WirePlumberWorker::dispatchMeterPoll(gpointer data)
{
    static_cast<WirePlumberWorker *>(data)->pollMeters();
    return G_SOURCE_CONTINUE;
}

void WirePlumberWorker::pollMeters()
{
    const QHash<QString, Level> readings = m_meters.takeReadings();
    QList<LevelReading> levels;
    levels.reserve(readings.size());
    // Declaration order, not hash order: a stable sequence makes the readings
    // comparable as a whole and keeps the wire payload deterministic.
    for (const BackendMeterTarget &target : std::as_const(m_declaredMetering)) {
        const auto it = readings.constFind(target.consoleId);
        if (it != readings.cend()) {
            levels.append(LevelReading{.id = target.consoleId, .level = *it});
        }
    }
    if (levels == m_lastLevels) {
        // Nothing moved. A silent console must not cost a signal every 50 ms.
        return;
    }
    m_lastLevels = levels;
    if (m_levelsCallback) {
        m_levelsCallback(std::move(levels));
    }
}

} // namespace QindaQt::Audio
