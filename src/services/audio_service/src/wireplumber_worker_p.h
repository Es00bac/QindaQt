// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_service/audio_backend.h>

#include <wp/wp.h>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace QindaQt::Audio
{

class WirePlumberWorker final
{
public:
    using SnapshotCallback = std::function<void(Snapshot)>;
    using OutcomeCallback = std::function<void(quint64, BackendOperationOutcome)>;

    WirePlumberWorker(quint64 initialEpoch, SnapshotCallback snapshotCallback,
                      OutcomeCallback outcomeCallback);
    ~WirePlumberWorker();

    void start();
    void stop();
    void submit(quint64 operationId, OperationRequest request);

private:
    struct OperationSync;

    void run();
    void setupCore();
    void finishApiLoading();
    void cleanupCore();
    void handleDisconnected();
    void scheduleReconnect();
    void rebuild();
    void publishUnavailable(const QString &reasonCode);
    void publish(Snapshot snapshot);
    void advanceEpoch();
    void invalidatePending(const QString &reasonCode);
    void submitOnWorker(quint64 operationId, const OperationRequest &request);
    void beginSync(quint64 operationId);
    void finishSync(quint64 operationId, quint64 operationEpoch, bool success);
    void cancelOperationSyncs();
    void quitWhenSyncsDrained();
    void invoke(std::function<void()> task);

    static void onComponentLoaded(GObject *source, GAsyncResult *result, gpointer data);
    static void onManagerInstalled(WpObjectManager *manager, gpointer data);
    static void onObjectsChanged(WpObjectManager *manager, gpointer data);
    static void onMixerChanged(WpPlugin *plugin, guint id, gpointer data);
    static void onDefaultsChanged(WpPlugin *plugin, gpointer data);
    static void onCoreDisconnected(WpCore *core, gpointer data);
    static void onCoreSync(GObject *source, GAsyncResult *result, gpointer data);

    quint64 m_epoch = 0;
    quint64 m_revision = 0;
    quint64 m_daemonSerial = 0;
    bool m_hadDaemon = false;
    bool m_managerInstalled = false;
    bool m_apiLoadFailed = false;
    guint m_pendingComponents = 0;
    bool m_resetScheduled = false;

    SnapshotCallback m_snapshotCallback;
    OutcomeCallback m_outcomeCallback;
    std::optional<Snapshot> m_lastSnapshot;
    std::unordered_map<quint64, quint64> m_pendingOperations;
    std::unordered_set<OperationSync *> m_operationSyncs;

    GMainContext *m_context = nullptr;
    GMainLoop *m_loop = nullptr;
    WpCore *m_core = nullptr;
    WpObjectManager *m_manager = nullptr;
    WpPlugin *m_mixer = nullptr;
    WpPlugin *m_defaultNodes = nullptr;

    std::mutex m_lifecycleMutex;
    std::condition_variable m_contextReady;
    std::thread m_thread;
    bool m_started = false;
    std::atomic_bool m_stopping = false;
};

} // namespace QindaQt::Audio
