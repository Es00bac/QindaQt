// SPDX-License-Identifier: GPL-3.0-or-later

#include "wireplumber_worker_p.h"

#include "wireplumber_graph_p.h"

#include <qindaqt/services/audio_protocol/audio_limits.h>

#include <pipewire/keys.h>

#include <QtCore/QByteArray>
#include <QtCore/QSet>
#include <QtCore/QStringList>

namespace QindaQt::Audio
{

struct WirePlumberWorker::OperationSync {
    WirePlumberWorker *worker = nullptr;
    quint64 operationId = 0;
    quint64 epoch = 0;
    GCancellable *cancellable = nullptr;
    bool cancelled = false;
    // Keeps the factory-created virtual node proxy alive across the core-sync
    // fence so the server-side global cannot outrun its creator reference.
    GObject *hold = nullptr;

    ~OperationSync()
    {
        g_clear_object(&cancellable);
        g_clear_object(&hold);
    }
};

struct WirePlumberWorker::NodeActivation {
    WirePlumberWorker *worker = nullptr;
    quint64 operationId = 0;
    quint64 epoch = 0;
    WpCore *core = nullptr;
    WpNode *node = nullptr;
    GCancellable *cancellable = nullptr;
    bool cancelled = false;

    ~NodeActivation()
    {
        g_clear_object(&cancellable);
        g_clear_object(&node);
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

QByteArray positionLabelsForChannels(const quint32 channels)
{
    switch (channels) {
    case 2:
        return QByteArrayLiteral("[ FL FR ]");
    case 4:
        return QByteArrayLiteral("[ FL FR FC LFE ]");
    case 6:
        return QByteArrayLiteral("[ FL FR FC LFE SL SR ]");
    case 8:
        return QByteArrayLiteral("[ FL FR FC LFE SL SR RL RR ]");
    default:
        return {};
    }
}

QString slugFromDisplayName(const QString &displayName)
{
    static constexpr qsizetype maxSlugLength = 48;
    QString slug;
    bool pendingDash = false;
    for (const QChar &character : displayName.toLower()) {
        const bool allowed = (character >= QLatin1Char('a') && character <= QLatin1Char('z'))
            || (character >= QLatin1Char('0') && character <= QLatin1Char('9'));
        if (allowed) {
            if (pendingDash && !slug.isEmpty()) {
                slug += QLatin1Char('-');
            }
            pendingDash = false;
            if (slug.size() < maxSlugLength) {
                slug += character;
            }
        } else {
            pendingDash = true;
        }
    }
    while (slug.endsWith(QLatin1Char('-'))) {
        slug.chop(1);
    }
    if (slug.isEmpty()) {
        slug = QStringLiteral("device");
    }
    return slug;
}

QSet<QString> nodeNames(WpObjectManager *manager)
{
    QSet<QString> names;
    WpIterator *iterator =
        wp_object_manager_new_filtered_iterator(manager, WP_TYPE_NODE, nullptr);
    GValue value = G_VALUE_INIT;
    while (wp_iterator_next(iterator, &value)) {
        auto *node = WP_PIPEWIRE_OBJECT(g_value_get_object(&value));
        const gchar *name = wp_pipewire_object_get_property(node, PW_KEY_NODE_NAME);
        if (name != nullptr && *name != '\0') {
            names.insert(QString::fromUtf8(name));
        }
        g_value_unset(&value);
    }
    wp_iterator_unref(iterator);
    return names;
}

QString freeVirtualNodeName(WpObjectManager *manager, const QString &displayName)
{
    const QString prefix = QString::fromLatin1(kVirtualDeviceNamePrefix);
    const QString base = prefix + slugFromDisplayName(displayName);
    const QSet<QString> taken = nodeNames(manager);
    QString candidate = base;
    for (quint32 attempt = 2; taken.contains(candidate); ++attempt) {
        candidate = base + QLatin1Char('-') + QString::number(attempt);
    }
    return candidate;
}

// Retained per-channel count for a target serial, from the last published
// snapshot. Zero means the worker has no channel truth for the target.
qsizetype retainedChannelCount(const Snapshot &snapshot, const quint64 serial)
{
    const auto inspectTarget = [serial](const auto &targets) -> qsizetype {
        for (const auto &target : targets) {
            if (target.handle.serial == serial) {
                return target.channelVolumes.size();
            }
        }
        return 0;
    };
    const qsizetype outputs = inspectTarget(snapshot.outputs);
    if (outputs != 0) {
        return outputs;
    }
    const qsizetype inputs = inspectTarget(snapshot.inputs);
    if (inputs != 0) {
        return inputs;
    }
    return inspectTarget(snapshot.streams);
}

} // namespace

void WirePlumberWorker::submitOnWorker(const quint64 operationId,
                                       const OperationRequest &request)
{
    // CreateVirtualDevice names a device that does not exist yet; every other
    // kind resolves a retained handle against the current worker epoch.
    const bool targeted = request.kind != OperationKind::CreateVirtualDevice;
    if ((targeted && request.primary.epoch != m_epoch) || m_daemonSerial == 0
        || m_core == nullptr || m_manager == nullptr) {
        m_outcomeCallback(operationId,
                          {.status = BackendOperationStatus::Failed,
                           .reasonCode = QStringLiteral("stale-handle"),
                           .diagnostic = {}});
        return;
    }

    std::optional<WirePlumberGraph::NodeLookup> primary;
    if (targeted) {
        primary = WirePlumberGraph::findNode(m_manager, request.primary.serial);
        if (!primary.has_value()) {
            m_outcomeCallback(operationId,
                              {.status = BackendOperationStatus::Failed,
                               .reasonCode = QStringLiteral("target-disappeared"),
                               .diagnostic = {}});
            return;
        }
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
    case OperationKind::SetChannelVolumes: {
        if (m_mixer == nullptr) {
            break;
        }
        // AGENT-GUARD: The write must cover exactly the channel topology the
        // last published snapshot advertised; mixer-api keeps unspecified
        // channels, so a silent count mismatch would leave stale per-channel
        // levels behind.
        const qsizetype retained = m_lastSnapshot.has_value()
            ? retainedChannelCount(*m_lastSnapshot, request.primary.serial)
            : 0;
        if (retained == 0 || request.channelVolumes.size() != retained) {
            m_outcomeCallback(operationId,
                              {.status = BackendOperationStatus::Failed,
                               .reasonCode = QStringLiteral("invalid-target"),
                               .diagnostic = {}});
            return;
        }
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
        // AGENT-CONTRACT: mixer-api set-volume accepts per-channel state as
        // {channelVolumes: a{sv}} keyed by decimal channel-index strings with
        // nested {"volume": d} values; plain double arrays are ignored.
        GVariantBuilder channels;
        g_variant_builder_init(&channels, G_VARIANT_TYPE("a{sv}"));
        for (qsizetype index = 0; index < request.channelVolumes.size(); ++index) {
            GVariantBuilder one;
            g_variant_builder_init(&one, G_VARIANT_TYPE_VARDICT);
            g_variant_builder_add(&one, "{sv}", "volume",
                                  g_variant_new_double(request.channelVolumes.at(index)));
            gchar key[16];
            g_snprintf(key, sizeof(key), "%lld", static_cast<long long>(index));
            g_variant_builder_add(&channels, "{sv}", key, g_variant_builder_end(&one));
        }
        g_variant_builder_add(&builder, "{sv}", "channelVolumes",
                              g_variant_builder_end(&channels));
        GVariant *dictionary = g_variant_builder_end(&builder);
        gboolean result = FALSE;
        g_signal_emit_by_name(m_mixer, "set-volume", primary->boundId, dictionary,
                              &result);
        g_variant_unref(dictionary);
        accepted = result != FALSE;
        break;
    }
    case OperationKind::CreateVirtualDevice: {
        const QByteArray positions = positionLabelsForChannels(request.channels);
        if (positions.isEmpty()) {
            break;
        }
        const QString nodeName = freeVirtualNodeName(m_manager, request.displayName);
        WpProperties *properties = wp_properties_new(
            "factory.name", "support.null-audio-sink", "node.name",
            nodeName.toUtf8().constData(), "node.description",
            request.displayName.toUtf8().constData(), "media.class",
            request.deviceKind == DeviceKind::Output ? "Audio/Sink" : "Audio/Source",
            "object.linger", "true", "audio.position", positions.constData(), nullptr);
        WpNode *node = wp_node_new_from_factory(m_core, "adapter", properties);
        if (node == nullptr) {
            break;
        }
        m_pendingOperations.insert_or_assign(operationId, m_epoch);
        beginNodeActivation(operationId, node);
        return;
    }
    case OperationKind::RemoveVirtualDevice: {
        // AGENT-GUARD: Never destroy a node outside the managed virtual
        // prefix; this is the backend-side fence backing the coordinator's
        // virtualDevice admission check. Monitor facets of managed sinks are
        // excluded alongside foreign nodes.
        if (!primary->nodeName.startsWith(QLatin1String(kVirtualDeviceNamePrefix))
            || primary->nodeName.endsWith(QLatin1String(".monitor"))) {
            m_outcomeCallback(operationId,
                              {.status = BackendOperationStatus::Failed,
                               .reasonCode = QStringLiteral("invalid-target"),
                               .diagnostic = {}});
            return;
        }
        wp_global_proxy_request_destroy(WP_GLOBAL_PROXY(primary->node));
        accepted = true;
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

void WirePlumberWorker::beginNodeActivation(const quint64 operationId, WpNode *node)
{
    auto *state = new NodeActivation{.worker = this,
                                     .operationId = operationId,
                                     .epoch = m_epoch,
                                     .core = m_core,
                                     .node = WP_NODE(node),
                                     .cancellable = g_cancellable_new(),
                                     .cancelled = false};
    m_nodeActivations.insert(state);
    wp_object_activate(WP_OBJECT(state->node), WP_PIPEWIRE_OBJECT_FEATURES_MINIMAL,
                       state->cancellable, onNodeActivated, state);
}

void WirePlumberWorker::onNodeActivated(GObject *source, GAsyncResult *result, gpointer data)
{
    std::unique_ptr<NodeActivation> state(static_cast<NodeActivation *>(data));
    WirePlumberWorker *worker = state->worker;
    worker->m_nodeActivations.erase(state.get());
    auto *node = WP_NODE(source);
    const bool success = state->cancelled ? false
                                          : wp_object_activate_finish(WP_OBJECT(node),
                                                                      result, nullptr) != FALSE;
    if (!state->cancelled && success && state->core == worker->m_core
        && state->epoch == worker->m_epoch) {
        worker->beginSync(state->operationId, G_OBJECT(state->node));
        state->node = nullptr;
        state->core = nullptr;
    } else if (!state->cancelled && state->core == worker->m_core) {
        worker->failPendingOperation(state->operationId,
                                     QStringLiteral("virtual-device-create-failed"));
    }
    worker->quitWhenCallbacksDrained();
}

void WirePlumberWorker::cancelNodeActivations()
{
    // AGENT-GUARD: The object activation owns callback data and the node
    // reference until finish runs. Cancel but retain every state, then keep
    // the GLib loop alive until callbacks consume them, mirroring the
    // component-load discipline.
    for (NodeActivation *state : m_nodeActivations) {
        state->cancelled = true;
        g_cancellable_cancel(state->cancellable);
    }
}

void WirePlumberWorker::failPendingOperation(const quint64 operationId, const QString &reasonCode)
{
    if (m_pendingOperations.erase(operationId) == 0) {
        return;
    }
    m_outcomeCallback(operationId,
                      {.status = BackendOperationStatus::Failed,
                       .reasonCode = reasonCode,
                       .diagnostic = {}});
}

void WirePlumberWorker::beginSync(const quint64 operationId, GObject *hold)
{
    auto *state = new OperationSync{.worker = this,
                                    .operationId = operationId,
                                    .epoch = m_epoch,
                                    .cancellable = g_cancellable_new(),
                                    .cancelled = false,
                                    .hold = hold};
    if (hold != nullptr) {
        g_object_ref(hold);
    }
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
    worker->quitWhenCallbacksDrained();
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

void WirePlumberWorker::quitWhenCallbacksDrained()
{
    if (m_stopping && m_componentLoads.empty() && m_nodeActivations.empty()
        && m_operationSyncs.empty() && m_loop != nullptr) {
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
