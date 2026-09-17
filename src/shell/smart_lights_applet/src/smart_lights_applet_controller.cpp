// SPDX-License-Identifier: GPL-3.0-or-later

#include "smart_lights_applet_controller.h"

#include "qindaqt/services/wiz_protocol/wiz_limits.h"
#include "qindaqt/services/wiz_protocol/wiz_scenes.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QVariantMap>

namespace QindaQt::Shell::SmartLightsApplet
{
namespace
{

[[nodiscard]] QString translate(const char *text)
{
    return QCoreApplication::translate("QindaQt::Shell::SmartLightsApplet", text);
}

[[nodiscard]] QVariantMap toVariant(const DeviceRow &row)
{
    QVariantMap map;
    map.insert(QStringLiteral("deviceId"), row.id);
    map.insert(QStringLiteral("label"), row.label);
    map.insert(QStringLiteral("statusLabel"), row.statusLabel);
    map.insert(QStringLiteral("on"), row.on);
    map.insert(QStringLiteral("reachable"), row.reachable);
    map.insert(QStringLiteral("controllable"), row.controllable);
    map.insert(QStringLiteral("capabilitiesKnown"), row.capabilitiesKnown);
    map.insert(QStringLiteral("supportsDimming"), row.supportsDimming);
    map.insert(QStringLiteral("brightnessPercent"), row.brightnessPercent);
    map.insert(QStringLiteral("minimumBrightnessPercent"), row.minimumBrightnessPercent);
    map.insert(QStringLiteral("supportsTemperature"), row.supportsTemperature);
    map.insert(QStringLiteral("temperatureKelvin"), row.temperatureKelvin);
    map.insert(QStringLiteral("minimumKelvin"), row.minimumKelvin);
    map.insert(QStringLiteral("maximumKelvin"), row.maximumKelvin);
    map.insert(QStringLiteral("supportsColor"), row.supportsColor);
    map.insert(QStringLiteral("colorHex"), row.colorHex);
    map.insert(QStringLiteral("supportsScenes"), row.supportsScenes);
    map.insert(QStringLiteral("sceneId"), row.sceneId);
    map.insert(QStringLiteral("sceneName"), row.sceneName);
    map.insert(QStringLiteral("supportsSpeed"), row.supportsSpeed);
    map.insert(QStringLiteral("speedPercent"), row.speedPercent);
    map.insert(QStringLiteral("speedApplies"), row.speedApplies);
    map.insert(QStringLiteral("signalKnown"), row.signalKnown);
    map.insert(QStringLiteral("signalDbm"), row.signalDbm);
    map.insert(QStringLiteral("accessibleName"), row.accessibleName);
    map.insert(QStringLiteral("accessibleDescription"), row.accessibleDescription);
    return map;
}

[[nodiscard]] QVariantMap toVariant(const PresetRow &row)
{
    QVariantMap map;
    map.insert(QStringLiteral("presetId"), row.id);
    map.insert(QStringLiteral("name"), row.name);
    map.insert(QStringLiteral("summary"), row.summary);
    map.insert(QStringLiteral("applicable"), row.applicable);
    map.insert(QStringLiteral("accessibleName"), row.accessibleName);
    map.insert(QStringLiteral("accessibleDescription"), row.accessibleDescription);
    return map;
}

} // namespace

SmartLightsAppletController::SmartLightsAppletController(
    Wiz::WizClient *client, SmartLights::ConfigurationStore *store,
    const bool readGranted, const bool controlGranted, QObject *parent)
    : QObject(parent)
    , m_client(client)
    , m_store(store)
    , m_readGranted(readGranted)
    , m_controlGranted(controlGranted)
{
    if (m_client != nullptr) {
        connect(m_client, &Wiz::WizClient::snapshotChanged, this,
                &SmartLightsAppletController::handleSnapshotChanged);
        connect(m_client, &Wiz::WizClient::operationCompleted, this,
                &SmartLightsAppletController::handleOperationCompleted);
        connect(m_client, &Wiz::WizClient::stateChanged, this,
                [this](Wiz::ClientState, const QString &) { reproject(); });
    }
    adoptStoredConfiguration();
    reproject();
}

SmartLightsAppletController::~SmartLightsAppletController() = default;

void SmartLightsAppletController::adoptStoredConfiguration()
{
    if (m_store == nullptr) {
        return;
    }
    QString error;
    m_configuration = m_store->load(&error);
    if (m_store->unreadable()) {
        publishFeedback(translate("Saved light settings could not be read."));
    }
    if (m_client == nullptr || !m_readGranted) {
        return;
    }
    for (const SmartLights::StoredDevice &device : m_configuration.devices) {
        // The stored address is a hint that saves a broadcast round; identity
        // still has to come from the device itself.
        m_client->seedDevice(device.mac, device.lastAddress);
        m_client->applyStoredLabel(device.mac, device.label);
    }
}

void SmartLightsAppletController::persistConfiguration()
{
    if (m_store == nullptr) {
        return;
    }
    QString error;
    if (!m_store->save(m_configuration, &error)) {
        publishFeedback(translate("Light settings could not be saved."));
    }
}

QString SmartLightsAppletController::rowIdForMac(const QString &mac)
{
    const auto existing = m_rowIds.constFind(mac);
    if (existing != m_rowIds.cend()) {
        return *existing;
    }
    const QString token = QStringLiteral("light-%1").arg(m_nextRowSerial++);
    m_rowIds.insert(mac, token);
    m_rowMacs.insert(token, mac);
    return token;
}

QString SmartLightsAppletController::macForRow(const QString &rowId) const
{
    return m_rowMacs.value(rowId);
}

std::optional<Wiz::Device> SmartLightsAppletController::deviceForRow(
    const QString &rowId) const
{
    const QString mac = macForRow(rowId);
    if (mac.isEmpty() || m_client == nullptr) {
        return std::nullopt;
    }
    const Wiz::Snapshot snapshot = m_client->snapshot();
    for (const Wiz::Device &device : snapshot.devices) {
        if (device.identity.mac == mac) {
            return device;
        }
    }
    return std::nullopt;
}

void SmartLightsAppletController::handleSnapshotChanged()
{
    if (m_client == nullptr) {
        return;
    }
    const Wiz::Snapshot snapshot = m_client->snapshot();

    // Remember where each known light answered, so the next session can reach
    // it without waiting for a broadcast round.
    bool configurationChanged = false;
    for (const Wiz::Device &device : snapshot.devices) {
        if (device.identity.address.isEmpty()) {
            continue;
        }
        bool found = false;
        for (SmartLights::StoredDevice &stored : m_configuration.devices) {
            if (stored.mac != device.identity.mac) {
                continue;
            }
            found = true;
            if (stored.lastAddress != device.identity.address) {
                stored.lastAddress = device.identity.address;
                configurationChanged = true;
            }
            break;
        }
        // AGENT-GUARD: the stored list stays inside the same bound the loader
        // enforces. Anyone on the network can answer discovery, and a document
        // that grows past what a reload keeps would silently lose entries.
        if (!found && m_configuration.devices.size() < Wiz::Limits::maximumDevices) {
            SmartLights::StoredDevice stored;
            stored.mac = device.identity.mac;
            stored.lastAddress = device.identity.address;
            m_configuration.devices.append(stored);
            configurationChanged = true;
        }
    }
    if (configurationChanged) {
        persistConfiguration();
    }

    // An epoch change means the client restarted; nothing pending survives it.
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        const RequestState observed = observeSmartLightAuthority(
            it.value(), snapshot.availability == Wiz::Availability::Ready,
            snapshot.epoch);
        if (observed.pending()) {
            ++it;
            continue;
        }
        if (!observed.feedback.isEmpty()) {
            publishFeedback(observed.feedback);
        }
        it = m_pending.erase(it);
    }

    reproject();
}

void SmartLightsAppletController::handleOperationCompleted(
    const quint64 requestId, const Wiz::OperationResult &result)
{
    const auto it = m_pending.find(requestId);
    if (it == m_pending.end()) {
        return;
    }
    const RequestState completed = applySmartLightResult(it.value(), result);
    if (completed.pending()) {
        return;
    }
    if (!completed.feedback.isEmpty()) {
        publishFeedback(completed.feedback);
    } else if (completed.phase == RequestPhase::Succeeded) {
        // A change that worked speaks for itself through the device rows; a
        // stale complaint from an earlier attempt must not outlive it.
        m_feedback.clear();
        Q_EMIT feedbackChanged();
    }
    m_pending.erase(it);
    reproject();
}

void SmartLightsAppletController::reproject()
{
    Wiz::Snapshot snapshot;
    if (m_client != nullptr && m_readGranted) {
        snapshot = m_client->snapshot();
    } else {
        snapshot.availability = Wiz::Availability::Unavailable;
    }
    for (const Wiz::Device &device : snapshot.devices) {
        static_cast<void>(rowIdForMac(device.identity.mac));
    }
    const SmartLightsAppletModel projected = projectSmartLightsApplet(
        snapshot, m_configuration.presets, m_rowIds, m_readGranted, m_controlGranted);
    if (projected == m_model) {
        return;
    }
    m_model = projected;
    Q_EMIT stateChanged();
}

QString SmartLightsAppletController::phase() const
{
    switch (m_model.phase) {
    case ServicePhase::Loading:
        return QStringLiteral("loading");
    case ServicePhase::Ready:
        return QStringLiteral("ready");
    case ServicePhase::Unavailable:
        return QStringLiteral("unavailable");
    }
    return QStringLiteral("unavailable");
}

QVariantList SmartLightsAppletController::deviceRows() const
{
    QVariantList rows;
    rows.reserve(m_model.devices.size());
    for (const DeviceRow &row : m_model.devices) {
        rows.append(toVariant(row));
    }
    return rows;
}

QVariantList SmartLightsAppletController::presetRows() const
{
    QVariantList rows;
    rows.reserve(m_model.presets.size());
    for (const PresetRow &row : m_model.presets) {
        rows.append(toVariant(row));
    }
    return rows;
}

QVariantList SmartLightsAppletController::sceneOptions(const QString &rowId) const
{
    QVariantList options;
    const auto device = deviceForRow(rowId);
    if (!device.has_value()) {
        return options;
    }
    const auto scenes = Wiz::scenesFor(device->features);
    options.reserve(scenes.size());
    for (const Wiz::SceneDescriptor &scene : scenes) {
        QVariantMap option;
        option.insert(QStringLiteral("sceneId"), static_cast<int>(scene.id));
        option.insert(QStringLiteral("name"), scene.name);
        option.insert(QStringLiteral("dynamic"), scene.dynamic);
        options.append(option);
    }
    return options;
}

void SmartLightsAppletController::setExpanded(const bool expanded)
{
    if (m_expanded == expanded) {
        return;
    }
    m_expanded = expanded;
    if (expanded) {
        static_cast<void>(requestRefresh());
    }
}

void SmartLightsAppletController::publishFeedback(const QString &message)
{
    if (m_feedback == message) {
        return;
    }
    m_feedback = message;
    Q_EMIT feedbackChanged();
}

void SmartLightsAppletController::clearFeedback()
{
    publishFeedback(QString());
}

bool SmartLightsAppletController::dispatch(const Wiz::OperationRequest &request)
{
    if (m_client == nullptr) {
        return false;
    }
    const Wiz::Snapshot snapshot = m_client->snapshot();
    const RequestState admitted =
        beginSmartLightRequest(snapshot, request, m_controlGranted);
    if (!admitted.pending()) {
        publishFeedback(admitted.feedback);
        return false;
    }
    const quint64 requestId = m_client->dispatch(request);
    m_pending.insert(requestId, admitted);
    reproject();
    Q_EMIT stateChanged();
    return true;
}

} // namespace QindaQt::Shell::SmartLightsApplet
