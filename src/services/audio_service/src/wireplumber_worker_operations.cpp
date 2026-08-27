// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_worker_p.h"

#include "wireplumber_graph_p.h"

#include <QtCore/QByteArray>

namespace QindaQt::Audio
{

struct WirePlumberWorker::OperationSync {
    WirePlumberWorker *worker = nullptr;
    quint64 operationId = 0;
    quint64 epoch = 0;
    GCancellable *cancellable = nullptr;
    bool cancelled = false;

    ~OperationSync()
    {
        g_clear_object(&cancellable);
    }
};

namespace
{

bool isOutputDevice(const QString &mediaClass)
{
    return mediaClass.startsWith(QStringLiteral("Audio/Sink"));
}

bool isInputDevice(const QString &mediaClass)
{
    return mediaClass.startsWith(QStringLiteral("Audio/Source"));
}

bool isPlaybackStream(const QString &mediaClass)
{
    return mediaClass.startsWith(QStringLiteral("Stream/Output/Audio"));
}

bool isCaptureStream(const QString &mediaClass)
{
    return mediaClass.startsWith(QStringLiteral("Stream/Input/Audio"));
}

} // namespace

void WirePlumberWorker::submitOnWorker(const quint64 operationId,
                                       const OperationRequest &request)
{
    if (request.primary.epoch != m_epoch || m_daemonSerial == 0 || m_core == nullptr
        || m_manager == nullptr) {
        m_outcomeCallback(operationId,
                          {.status = BackendOperationStatus::Failed,
                           .reasonCode = QStringLiteral("stale-handle"),
                           .diagnostic = {}});
        return;
    }

    auto primary = WirePlumberGraph::findNode(m_manager, request.primary.serial);
    if (!primary.has_value()) {
        m_outcomeCallback(operationId,
                          {.status = BackendOperationStatus::Failed,
                           .reasonCode = QStringLiteral("target-disappeared"),
                           .diagnostic = {}});
        return;
    }

    bool accepted = false;
    switch (request.kind) {
    case OperationKind::SetDefault: {
        if (m_defaultNodes == nullptr || primary->nodeName.isEmpty()
            || (!isOutputDevice(primary->mediaClass)
                && !isInputDevice(primary->mediaClass))) {
            break;
        }
        const QByteArray mediaClass = isOutputDevice(primary->mediaClass)
            ? QByteArrayLiteral("Audio/Sink")
            : QByteArrayLiteral("Audio/Source");
        const QByteArray nodeName = primary->nodeName.toUtf8();
        gboolean result = FALSE;
        g_signal_emit_by_name(m_defaultNodes, "set-default-configured-node-name",
                              mediaClass.constData(), nodeName.constData(), &result);
        accepted = result != FALSE;
        break;
    }
    case OperationKind::SetVolume:
    case OperationKind::SetMute: {
        if (m_mixer == nullptr) {
            break;
        }
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
        if (request.kind == OperationKind::SetVolume) {
            g_variant_builder_add(&builder, "{sv}", "volume",
                                  g_variant_new_double(request.volume));
        } else {
            g_variant_builder_add(&builder, "{sv}", "mute",
                                  g_variant_new_boolean(request.muted));
        }
        GVariant *dictionary = g_variant_builder_end(&builder);
        gboolean result = FALSE;
        g_signal_emit_by_name(m_mixer, "set-volume", primary->boundId, dictionary,
                              &result);
        g_variant_unref(dictionary);
        accepted = result != FALSE;
        break;
    }
    case OperationKind::MoveStream: {
        if (request.secondary.epoch != m_epoch) {
            break;
        }
        auto device = WirePlumberGraph::findNode(m_manager, request.secondary.serial);
        const bool compatible = device.has_value()
            && ((isPlaybackStream(primary->mediaClass) && isOutputDevice(device->mediaClass))
                || (isCaptureStream(primary->mediaClass)
                    && isInputDevice(device->mediaClass)));
        if (!compatible) {
            break;
        }
        WpMetadata *metadata = WirePlumberGraph::defaultMetadata(m_manager);
        if (metadata == nullptr) {
            break;
        }
        const QByteArray serial = QByteArray::number(request.secondary.serial);
        wp_metadata_set(metadata, primary->boundId, "target.object", "Spa:Id",
                        serial.constData());
        g_object_unref(metadata);
        accepted = true;
        break;
    }
    }

    if (!accepted) {
        m_outcomeCallback(operationId,
                          {.status = BackendOperationStatus::Unsupported,
                           .reasonCode = QStringLiteral("operation-unsupported"),
                           .diagnostic = {}});
        return;
    }
    m_pendingOperations.insert_or_assign(operationId, m_epoch);
    beginSync(operationId);
}

void WirePlumberWorker::beginSync(const quint64 operationId)
{
    auto *state = new OperationSync{.worker = this,
                                    .operationId = operationId,
                                    .epoch = m_epoch,
                                    .cancellable = g_cancellable_new(),
                                    .cancelled = false};
    m_operationSyncs.insert(state);
    if (!wp_core_sync(m_core, state->cancellable, onCoreSync, state)) {
        m_operationSyncs.erase(state);
        delete state;
        finishSync(operationId, m_epoch, false);
    }
}

void WirePlumberWorker::onCoreSync(GObject *source, GAsyncResult *result, gpointer data)
{
    std::unique_ptr<OperationSync> state(static_cast<OperationSync *>(data));
    WirePlumberWorker *worker = state->worker;
    worker->m_operationSyncs.erase(state.get());
    auto *core = WP_CORE(source);
    GError *error = nullptr;
    const bool success = wp_core_sync_finish(core, result, &error) != FALSE;
    g_clear_error(&error);
    if (!state->cancelled) {
        worker->finishSync(state->operationId, state->epoch, success);
    }
    worker->quitWhenSyncsDrained();
}

void WirePlumberWorker::cancelOperationSyncs()
{
    // AGENT-GUARD: wp_core_sync owns the callback data until its asynchronous
    // completion. Cancel but do not delete states; stop keeps the GLib loop
    // alive until every callback has released its state, preventing both UAF
    // and callback-data leaks across core replacement.
    for (OperationSync *state : m_operationSyncs) {
        state->cancelled = true;
        g_cancellable_cancel(state->cancellable);
    }
}

void WirePlumberWorker::quitWhenSyncsDrained()
{
    if (m_stopping && m_operationSyncs.empty() && m_loop != nullptr) {
        g_main_loop_quit(m_loop);
    }
}

void WirePlumberWorker::finishSync(const quint64 operationId,
                                   const quint64 operationEpoch, const bool success)
{
    const auto it = m_pendingOperations.find(operationId);
    if (it == m_pendingOperations.end()) {
        return;
    }
    m_pendingOperations.erase(it);
    if (!success || operationEpoch != m_epoch || m_daemonSerial == 0) {
        m_outcomeCallback(operationId,
                          {.status = BackendOperationStatus::Uncertain,
                           .reasonCode = QStringLiteral("operation-sync-uncertain"),
                           .diagnostic = {}});
        rebuild();
        return;
    }
    rebuild();
    m_outcomeCallback(operationId,
                      {.status = BackendOperationStatus::Succeeded,
                       .reasonCode = QStringLiteral("ok"),
                       .diagnostic = {}});
}

} // namespace QindaQt::Audio
