// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_backend.h>

#include "wireplumber_meters_p.h"

#include <wp/wp.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace QindaQt::Audio
{

// Private lifecycle observation used only by the deterministic worker tests.
// Production constructs the empty default and pays no cross-thread callback.
struct WirePlumberWorkerLifecycleHooks {
    std::function<void()> disconnectResetScheduled;
    std::function<void()> stopTaskQueued;
};

class WirePlumberWorker final
{
public:
    using SnapshotCallback = std::function<void(Snapshot)>;
    using OutcomeCallback = std::function<void(quint64, BackendOperationOutcome)>;
    using LevelsCallback = std::function<void(QList<LevelReading>)>;

    WirePlumberWorker(quint64 initialEpoch, SnapshotCallback snapshotCallback,
                      OutcomeCallback outcomeCallback, LevelsCallback levelsCallback,
                      WirePlumberWorkerLifecycleHooks lifecycleHooks = {});
    ~WirePlumberWorker();

    void start();
    void stop();
    void submit(quint64 operationId, OperationRequest request);
    // Declares the console's complete routing (ADR-0173). Called from the Qt
    // thread; the work is marshalled onto the worker thread like every other
    // graph mutation.
    void applyRouting(QList<BackendRoutingEdge> edges);
    // Declares the console's complete metering (ADR-0174), marshalled onto the
    // worker thread the same way routing is.
    void applyMetering(QList<BackendMeterTarget> targets);
    // Declares the console's virtual endpoints (ADR-0175), marshalled onto the
    // worker thread like routing and metering.
    void applyConsoleEndpoints(QList<BackendConsoleEndpoint> endpoints);
    // Declares the active strip racks (ADR-0179), marshalled like the rest.
    void applyProcessing(QList<BackendProcessingChain> chains);
    void applyBusProcessing(QList<BackendBusChain> chains);
    // Called from a module's own destroy event: PipeWire took it down (a
    // stream that could not connect). Drops the entry only if it still holds
    // THAT module - the key may already belong to its replacement. Public
    // only because the event lands in a free C callback.
    enum class ModuleKind { Send, Chain, BusChain };
    void forgetModule(ModuleKind kind, const std::string &key, void *module);
    // Attaches the destroy listener; every loaded module goes through it.
    void watchModule(ModuleKind kind, const std::string &key, void *module);

private:
    struct ComponentLoad;
    struct DisconnectReset;
    struct NodeActivation;
    struct OperationSync;

    void run();
    void setupCore();
    void finishApiLoading();
    void beginComponentLoad(const char *component);
    void cancelDisconnectReset();
    void cleanupCore();
    void handleDisconnected(quint64 workerRun);
    void scheduleReconnect();
    void rebuild();
    void publishUnavailable(const QString &reasonCode);
    void publish(Snapshot snapshot);
    void advanceEpoch();
    void invalidatePending(const QString &reasonCode);
    void submitOnWorker(quint64 operationId, const OperationRequest &request);
    // Fails a console operation that reached the graph backend by mistake.
    void rejectConsoleOperation(quint64 operationId);
    // Diffs the declared routing against the loopback modules this worker has
    // loaded, then loads and unloads exactly the difference.
    void applyRoutingOnWorker(const QList<BackendRoutingEdge> &edges);
    void unloadAllRouting();
    // Rebuilds the meter capture streams to match the declaration, and starts
    // or stops the poll timer according to whether anything is metered.
    void applyMeteringOnWorker(const QList<BackendMeterTarget> &targets);
    // Creates every declared endpoint the graph does not already have: a
    // strip's sink server-side through the raw core API, a bus as a loopback
    // module in this worker's context.
    void applyConsoleEndpointsOnWorker(const QList<BackendConsoleEndpoint> &endpoints);
    void unloadAllEndpoints();
    // Loads, replaces and unloads one filter-chain per active rack so that the
    // running chains match the declaration; a changed rack is a reload.
    void applyProcessingOnWorker(const QList<BackendProcessingChain> &chains);
    void unloadAllProcessing();
    void applyBusProcessingOnWorker(const QList<BackendBusChain> &chains);
    // Where a send into a bus should play: the bus's own sink when the bus
    // has a running rack, otherwise the device. Empty when neither exists.
    [[nodiscard]] QString busWriteNode(const QString &busId, const Handle &device) const;
    // AGENT-GUARD: an impl module is never destroyed from inside a PipeWire or
    // WirePlumber dispatch. Every worker mutation runs in an objects-changed
    // callback, and destroying a module's client-node streams while the
    // protocol dispatch that delivered the callback is still iterating them
    // is a use-after-free inside libpipewire (seen as a SIGSEGV in
    // audioconvert when a send was reloaded onto a freshly loaded rack). The
    // module is handed to an idle source on the worker context and destroyed
    // on the next loop iteration instead.
    void destroyModuleLater(void *module);
    void flushPendingModuleDestroys();
    static gboolean dispatchModuleDestroys(gpointer data);
    // The node a strip's sends and meter should read: the processed sink when
    // the strip has a running rack, otherwise its device. Empty when the
    // device is not in the graph.
    [[nodiscard]] QString stripReadNode(const QString &stripId, const Handle &device,
                                        bool *readsSink) const;
    void startMeterPolling();
    void stopMeterPolling();
    void pollMeters();
    // Applies each declared send's gain to its loopback's playback node.
    void applySendVolumes();
    [[nodiscard]] QString nodeNameForHandle(const Handle &handle) const;
    void beginNodeActivation(quint64 operationId, WpNode *node);
    void failPendingOperation(quint64 operationId, const QString &reasonCode);
    void beginSync(quint64 operationId, GObject *hold = nullptr);
    void finishSync(quint64 operationId, quint64 operationEpoch, bool success);
    void cancelComponentLoads();
    void cancelNodeActivations();
    void cancelOperationSyncs();
    void quitWhenCallbacksDrained();
    void invoke(std::function<void()> task);

    static void onComponentLoaded(GObject *source, GAsyncResult *result, gpointer data);
    static void onManagerInstalled(WpObjectManager *manager, gpointer data);
    static void onObjectsChanged(WpObjectManager *manager, gpointer data);
    static void onMixerChanged(WpPlugin *plugin, guint id, gpointer data);
    static void onDefaultsChanged(WpPlugin *plugin, gpointer data);
    static void onCoreDisconnected(WpCore *core, gpointer data);
    static void onCoreSync(GObject *source, GAsyncResult *result, gpointer data);
    static void onNodeActivated(GObject *source, GAsyncResult *result, gpointer data);
    static gboolean dispatchMeterPoll(gpointer data);
    static gboolean dispatchDisconnectReset(gpointer data);
    static void deleteDisconnectReset(gpointer data);

    quint64 m_epoch = 0;
    quint64 m_revision = 0;
    quint64 m_daemonSerial = 0;
    quint64 m_workerRun = 0;
    bool m_hadDaemon = false;
    bool m_hasRun = false;
    bool m_managerInstalled = false;
    bool m_apiLoadFailed = false;
    guint m_pendingComponents = 0;

    SnapshotCallback m_snapshotCallback;
    OutcomeCallback m_outcomeCallback;
    LevelsCallback m_levelsCallback;
    WirePlumberWorkerLifecycleHooks m_lifecycleHooks;
    std::optional<Snapshot> m_lastSnapshot;
    std::unordered_map<quint64, quint64> m_pendingOperations;
    std::unordered_set<ComponentLoad *> m_componentLoads;
    std::unordered_set<NodeActivation *> m_nodeActivations;
    std::unordered_set<OperationSync *> m_operationSyncs;

    GMainContext *m_context = nullptr;
    GMainLoop *m_loop = nullptr;
    WpCore *m_core = nullptr;
    WpObjectManager *m_manager = nullptr;
    WpPlugin *m_mixer = nullptr;
    WpPlugin *m_defaultNodes = nullptr;
    GSource *m_disconnectResetSource = nullptr;
    // A module this worker loaded, with the argument it was loaded with, so a
    // changed endpoint is detected as a difference and reloaded. Diffing by
    // name alone kept a send on its old device after a re-pin or a rack
    // starting, because the send's NAME never changes.
    struct LoadedModule {
        void *module = nullptr;
        QByteArray arguments;
    };
    // Loopback modules this worker loaded, keyed by the send's node name.
    std::unordered_map<std::string, LoadedModule> m_routingModules;
    QList<BackendRoutingEdge> m_declaredRouting;
    QList<BackendMeterTarget> m_declaredMetering;
    QList<BackendConsoleEndpoint> m_declaredEndpoints;
    QList<BackendProcessingChain> m_declaredProcessing;
    // Filter-chain modules this worker loaded, keyed by strip id, with the
    // argument they were loaded with so a changed rack is detected as a
    // difference rather than re-derived.
    std::unordered_map<std::string, LoadedModule> m_processingModules;
    QList<BackendBusChain> m_declaredBusProcessing;
    std::unordered_map<std::string, LoadedModule> m_busProcessingModules;
    std::vector<void *> m_modulesPendingDestroy;
    GSource *m_moduleDestroySource = nullptr;
    // Bus loopback modules this worker loaded, keyed by the bus sink's node
    // name; destroyed with the core like the send loopbacks. The record, not
    // the graph, says an endpoint is already on its way: a new node takes
    // several rebuilds to be announced, and asking the graph in that window
    // would create a copy per rebuild.
    std::unordered_map<std::string, void *> m_endpointModules;
    // Raw proxies for the strip sinks this core asked the daemon for, keyed
    // the same way. Destroying a proxy does not destroy its lingering node;
    // the map exists for the duplicate guard and to release the proxies with
    // the core.
    std::unordered_map<std::string, void *> m_endpointProxies;
    MeterBank m_meters;
    GSource *m_meterSource = nullptr;
    // The last set of readings handed upstream. Identical readings are not
    // resent, so a silent console costs nothing on the bus.
    QList<LevelReading> m_lastLevels;

    std::mutex m_lifecycleMutex;
    std::condition_variable m_contextReady;
    std::thread m_thread;
    bool m_started = false;
    std::atomic_bool m_stopping = false;
};

} // namespace QindaQt::Audio
