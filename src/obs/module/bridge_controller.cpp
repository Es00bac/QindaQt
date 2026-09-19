// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge_controller.h"

#include "console_dock.h"

#include <qindaqt/services/audio_client/qt_audio_transport.h>

#include <obs.h>

#include <QDBusConnection>
#include <QHash>
#include <QMutexLocker>

namespace QindaQt::ObsBridge {
namespace {

struct Enumeration {
    QList<ExistingSource> existing;
    // Keyed by console id; a duplicate is keyed "<id>#<name>" exactly as the
    // plan names it for removal.
    QHash<QString, obs_source_t *> refs;
};

QString settingString(obs_data_t *settings, const char *key)
{
    return QString::fromUtf8(obs_data_get_string(settings, key));
}

void writeSettings(obs_data_t *settings, const DesiredSource &source)
{
    obs_data_set_string(settings, SettingsKeys::ConsoleId, source.consoleId.toUtf8().constData());
    obs_data_set_string(settings, SettingsKeys::Code, source.code.toUtf8().constData());
    obs_data_set_string(settings, SettingsKeys::Label, source.label.toUtf8().constData());
    obs_data_set_string(settings, SettingsKeys::CaptureDevice,
                        source.captureDevice.toUtf8().constData());
    obs_data_set_string(settings, SettingsKeys::CaptureKind,
                        captureKindToken(source.captureKind).toUtf8().constData());
}

bool collectSource(void *param, obs_source_t *source)
{
    auto *enumeration = static_cast<Enumeration *>(param);
    const auto kind = sourceKindFromId(QString::fromUtf8(obs_source_get_unversioned_id(source)));
    // A source OBS has already removed may linger while a holder still
    // references it; it is gone from the user's view and never adopted.
    if (!kind || obs_source_removed(source)) {
        return true;
    }
    obs_data_t *settings = obs_source_get_settings(source);
    ExistingSource existing;
    existing.consoleId = settingString(settings, SettingsKeys::ConsoleId);
    existing.kind = *kind;
    existing.sourceName = QString::fromUtf8(obs_source_get_name(source));
    existing.captureKind = captureKindFromToken(settingString(settings, SettingsKeys::CaptureKind))
                               .value_or(CaptureKind::None);
    existing.captureDevice = settingString(settings, SettingsKeys::CaptureDevice);
    obs_data_release(settings);
    // A bridge-typed source without a console id was made by hand; it is not
    // the bridge's to manage.
    if (existing.consoleId.isEmpty()) {
        return true;
    }
    const QString key = enumeration->refs.contains(existing.consoleId)
        ? existing.consoleId + QStringLiteral("#") + existing.sourceName
        : existing.consoleId;
    enumeration->refs.insert(key, obs_source_get_ref(source));
    enumeration->existing.append(existing);
    return true;
}

bool attachedChannel(obs_source_t *source, uint32_t *channel)
{
    for (uint32_t index = 1; index < MAX_CHANNELS; ++index) {
        obs_source_t *current = obs_get_output_source(index);
        const bool same = current == source;
        if (current != nullptr) {
            obs_source_release(current);
        }
        if (same) {
            *channel = index;
            return true;
        }
    }
    return false;
}

void attachToFreeChannel(obs_source_t *source)
{
    uint32_t channel = 0;
    if (attachedChannel(source, &channel)) {
        return;
    }
    for (uint32_t index = BridgeController::FirstChannel; index < MAX_CHANNELS; ++index) {
        obs_source_t *current = obs_get_output_source(index);
        if (current == nullptr) {
            obs_set_output_source(index, source);
            return;
        }
        obs_source_release(current);
    }
    blog(LOG_WARNING, "[obs-qindaqt] no free mixer channel for '%s'", obs_source_get_name(source));
}

void detachFromChannels(obs_source_t *source)
{
    uint32_t channel = 0;
    while (attachedChannel(source, &channel)) {
        obs_set_output_source(channel, nullptr);
    }
}

void updateSourceSettings(obs_source_t *source, const DesiredSource &desired)
{
    obs_data_t *update = obs_data_create();
    writeSettings(update, desired);
    obs_source_update(source, update);
    obs_data_release(update);
}

QString stateToken(Audio::ClientState state)
{
    switch (state) {
    case Audio::ClientState::Stopped:
        return QStringLiteral("stopped");
    case Audio::ClientState::Starting:
        return QStringLiteral("starting");
    case Audio::ClientState::Ready:
        return QStringLiteral("ready");
    case Audio::ClientState::Unavailable:
        return QStringLiteral("unavailable");
    case Audio::ClientState::Degraded:
        return QStringLiteral("degraded");
    }
    return QStringLiteral("unknown");
}

} // namespace

BridgeController::BridgeController(QObject *parent)
    : QObject(parent)
{
}

BridgeController::~BridgeController()
{
    stop();
}

void BridgeController::start()
{
    if (m_client) {
        return;
    }
    m_transport = std::make_unique<Audio::QtAudioTransport>(QDBusConnection::sessionBus());
    m_client = std::make_unique<Audio::AudioClient>(m_transport.get());
    connect(m_client.get(), &Audio::AudioClient::snapshotChanged, this,
            &BridgeController::onSnapshot);
    connect(m_client.get(), &Audio::AudioClient::stateChanged, this, &BridgeController::onState);
    connect(m_client.get(), &Audio::AudioClient::levelsChanged, this,
            &BridgeController::onLevels);
    if (obs_frontend_get_main_window() != nullptr) {
        // Loading a scene collection replaces every source, so the first
        // sync waits for the frontend to finish; the dock is the frontend's
        // to own from here on.
        m_frontendReady = false;
        m_frontendCallbacks = true;
        obs_frontend_add_event_callback(frontendEvent, this);
        auto *dock = new ConsoleDock;
        if (obs_frontend_add_dock_by_id("qindaqt-console", "QindaQt Console", dock)) {
            m_dock = dock;
        } else {
            delete dock;
        }
    }
    m_client->start();
}

void BridgeController::stop()
{
    if (m_frontendCallbacks) {
        obs_frontend_remove_event_callback(frontendEvent, this);
        m_frontendCallbacks = false;
    }
    m_dock = nullptr;
    if (m_client) {
        m_client->stop();
        m_client.reset();
    }
    m_transport.reset();
}

void BridgeController::frontendEvent(enum obs_frontend_event event, void *data)
{
    auto *self = static_cast<BridgeController *>(data);
    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
        self->m_frontendReady = true;
        // A restored collection can contain bridge sources from a retired
        // Audio1 owner. Without current authority they must not resume capture.
        self->applySnapshot(self->m_haveSnapshot ? self->m_lastSnapshot
                                                : Audio::Snapshot{});
        break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
    case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
    case OBS_FRONTEND_EVENT_EXIT:
        // Sources are about to be replaced or torn down: hands off until
        // the frontend says the new set is loaded.
        self->m_frontendReady = false;
        break;
    default:
        break;
    }
}

void BridgeController::onSnapshot(const Audio::Snapshot &snapshot)
{
    m_lastSnapshot = snapshot;
    m_haveSnapshot = true;
    if (m_frontendReady) {
        applySnapshot(snapshot);
    }
}

void BridgeController::onState(Audio::ClientState state, const QString &reasonCode)
{
    if (m_client && !m_client->hasSnapshot()) {
        // AGENT-CONTRACT: AudioClient discards its snapshot on owner loss.
        // Forget the same authority here, including the copy used after a
        // scene-collection switch. A failed refresh with retained truth does
        // not take this path. See architecture/obs-bridge.md.
        m_haveSnapshot = false;
        m_lastSnapshot = {};
        if (m_frontendReady) {
            applySnapshot({});
        }
        if (m_dock != nullptr) {
            m_dock->setSources({});
        }
    }
    {
        QMutexLocker lock(&m_mutex);
        if (!m_haveSnapshot) {
            m_mapping.sources.clear();
            m_mapping.epoch = 0;
            m_mapping.revision = 0;
        }
        m_mapping.audioState = stateToken(state);
        m_mapping.reasonCode = reasonCode;
    }
    if (m_dock != nullptr) {
        m_dock->setServiceState(stateToken(state), reasonCode);
    }
    Q_EMIT mappingChanged();
}

void BridgeController::onLevels(const QList<Audio::LevelReading> &levels)
{
    if (m_dock != nullptr) {
        m_dock->setLevels(levels);
    }
}

void BridgeController::applySnapshot(const Audio::Snapshot &snapshot)
{
    const QList<DesiredSource> desired = desiredSources(snapshot);
    Enumeration enumeration;
    obs_enum_sources(collectSource, &enumeration);
    const SyncPlan plan = planSync(desired, enumeration.existing);

    for (const QString &key : plan.removals) {
        obs_source_t *source = enumeration.refs.take(key);
        if (source == nullptr) {
            continue;
        }
        detachFromChannels(source);
        obs_source_remove(source);
        obs_source_release(source);
    }
    for (const Rename &rename : plan.renames) {
        if (obs_source_t *source = enumeration.refs.value(rename.consoleId)) {
            obs_source_set_name(source, rename.to.toUtf8().constData());
        }
    }
    for (const DesiredSource &retarget : plan.retargets) {
        if (obs_source_t *source = enumeration.refs.value(retarget.consoleId)) {
            updateSourceSettings(source, retarget);
        }
    }
    for (const DesiredSource &creation : plan.creations) {
        obs_data_t *settings = obs_data_create();
        writeSettings(settings, creation);
        obs_source_t *source = obs_source_create(sourceKindId(creation.kind).toUtf8().constData(),
                                                 creation.sourceName.toUtf8().constData(),
                                                 settings, nullptr);
        obs_data_release(settings);
        if (source == nullptr) {
            blog(LOG_WARNING, "[obs-qindaqt] could not create '%s'",
                 creation.sourceName.toUtf8().constData());
            continue;
        }
        enumeration.refs.insert(creation.consoleId, source);
    }
    // Every console source carries the console's current code and label and
    // sits on a mixer channel, including sources restored from a collection.
    for (const DesiredSource &source : desired) {
        obs_source_t *handle = enumeration.refs.value(source.consoleId);
        if (handle == nullptr) {
            continue;
        }
        obs_data_t *settings = obs_source_get_settings(handle);
        const bool stale = settingString(settings, SettingsKeys::Code) != source.code
            || settingString(settings, SettingsKeys::Label) != source.label;
        obs_data_release(settings);
        if (stale) {
            updateSourceSettings(handle, source);
        }
        attachToFreeChannel(handle);
    }
    for (obs_source_t *handle : std::as_const(enumeration.refs)) {
        obs_source_release(handle);
    }

    {
        QMutexLocker lock(&m_mutex);
        m_mapping.sources = desired;
        m_mapping.epoch = snapshot.epoch;
        m_mapping.revision = snapshot.revision;
    }
    if (m_dock != nullptr) {
        m_dock->setSources(desired);
    }
    Q_EMIT mappingChanged();
}

ConsoleMapping BridgeController::mapping() const
{
    QMutexLocker lock(&m_mutex);
    return m_mapping;
}

} // namespace QindaQt::ObsBridge
