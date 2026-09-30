// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/night_light/night_light_settings_importer.h>

#include <qindaqt/services/night_light/night_light_config_port.h>
#include <qindaqt/services/night_light/night_light_values.h>
#include <qindaqt/services/settings_client/settings_client.h>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

#include <QVariantMap>

namespace QindaQt::Services::NightLight {
namespace {
const QString marker = QStringLiteral("display.nightLight.legacyImported");
const QStringList importedKeys{
    QStringLiteral("display.nightLight.active"),
    QStringLiteral("display.nightLight.mode"),
    QStringLiteral("display.nightLight.dayTemperatureKelvin"),
    QStringLiteral("display.nightLight.nightTemperatureKelvin"),
    QStringLiteral("display.nightLight.scheduleSource"),
    QStringLiteral("display.nightLight.automaticLocation"),
    QStringLiteral("display.nightLight.latitudeDegrees"),
    QStringLiteral("display.nightLight.longitudeDegrees"),
    QStringLiteral("display.nightLight.sunriseStart"),
    QStringLiteral("display.nightLight.sunsetStart"),
    QStringLiteral("display.nightLight.transitionSeconds")};

QVariantMap legacyValues(const NightLightSettings &settings)
{
    return {
        {importedKeys.at(0), settings.output.active},
        {importedKeys.at(1), modeToConfigToken(settings.output.mode)},
        {importedKeys.at(2), settings.output.dayTemperatureKelvin},
        {importedKeys.at(3), settings.output.nightTemperatureKelvin},
        {importedKeys.at(4), sourceToConfigToken(settings.schedule.source)},
        {importedKeys.at(5), settings.schedule.automaticLocation},
        {importedKeys.at(6), settings.schedule.latitudeDegrees},
        {importedKeys.at(7), settings.schedule.longitudeDegrees},
        {importedKeys.at(8), settings.schedule.sunriseStart.toString(QStringLiteral("HH:mm:ss"))},
        {importedKeys.at(9), settings.schedule.sunsetStart.toString(QStringLiteral("HH:mm:ss"))},
        {importedKeys.at(10), settings.schedule.transitionSeconds}};
}
}

class NightLightSettingsImporter::Private final {
public:
    Private(SettingsClient::SettingsClient &client, NightLightConfigPort &reader)
        : settings(client), legacyReader(reader) {}
    SettingsClient::SettingsClient &settings;
    NightLightConfigPort &legacyReader;
    Status status = Status::Idle;
    QString message;
    QStringList pending;
    QVariantMap values;
    qsizetype index = 0;
    bool started = false;
    bool writing = false;
    bool waitingRefresh = false;
    bool retrying = false;
    quint64 minimumRevision = 0;
};

NightLightSettingsImporter::NightLightSettingsImporter(
    SettingsClient::SettingsClient &settings, NightLightConfigPort &legacyReader,
    QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(settings, legacyReader))
{
    connect(&settings, &SettingsClient::SettingsClient::snapshotChanged, this, [this] {
        const auto &snapshot = d->settings.snapshot();
        if (!snapshot) return;
        if (d->waitingRefresh
            && snapshot->revision >= d->minimumRevision) {
            d->waitingRefresh = false;
            if (d->retrying) {
                d->retrying = false;
                attempt(snapshot->values, snapshot->sourceLayers);
            } else {
                writeNext();
            }
        }
    });
    connect(&settings, &SettingsClient::SettingsClient::stateChanged, this, [this] {
        const auto &snapshot = d->settings.snapshot();
        if (!d->started || d->settings.state() != SettingsClient::ClientState::Ready
            || !snapshot)
            return;
        if (d->waitingRefresh
            && snapshot->revision >= d->minimumRevision) {
            d->waitingRefresh = false;
            if (d->retrying) {
                d->retrying = false;
                attempt(snapshot->values, snapshot->sourceLayers);
            } else {
                writeNext();
            }
        } else if (d->status == Status::Idle) {
            attempt(snapshot->values, snapshot->sourceLayers);
        }
    });
    connect(&settings, &SettingsClient::SettingsClient::commitFinished, this,
            [this](const SettingsClient::CommitOutcome &outcome) {
        if (!d->writing) return;
        d->writing = false;
        if (outcome.status != SettingsProtocol::SettingsWireStatus::Applied) {
            d->status = Status::SaveFailed;
            d->message = outcome.message.isEmpty()
                ? QStringLiteral("Legacy night light settings were not saved.")
                : outcome.message;
            d->pending.clear();
            d->index = 0;
            Q_EMIT stateChanged();
            return;
        }
        ++d->index;
        d->minimumRevision = outcome.revisionAfter;
        d->waitingRefresh = true;
    });
    connect(&settings, &SettingsClient::SettingsClient::commitUncertain, this,
            [this](const QString &message) {
        if (!d->writing) return;
        d->writing = false;
        d->status = Status::RetryRequired;
        d->message = message;
        d->pending.clear();
        d->index = 0;
        Q_EMIT stateChanged();
    });
}

NightLightSettingsImporter::~NightLightSettingsImporter() = default;

void NightLightSettingsImporter::start()
{
    if (d->started) return;
    d->started = true;
    const auto &snapshot = d->settings.snapshot();
    if (d->settings.state() == SettingsClient::ClientState::Ready && snapshot)
        attempt(snapshot->values, snapshot->sourceLayers);
}

void NightLightSettingsImporter::retry()
{
    if (!d->started || d->writing) return;
    d->status = Status::Importing;
    d->message.clear();
    d->waitingRefresh = false;
    d->retrying = false;
    Q_EMIT stateChanged();
    if (d->settings.state() == SettingsClient::ClientState::Ready
        && d->settings.snapshot()) {
        const auto &snapshot = *d->settings.snapshot();
        attempt(snapshot.values, snapshot.sourceLayers);
        return;
    }
    d->waitingRefresh = true;
    d->retrying = true;
    d->settings.refresh();
}

NightLightSettingsImporter::Status NightLightSettingsImporter::status() const noexcept
{
    return d->status;
}

QString NightLightSettingsImporter::message() const
{
    return d->message;
}

void NightLightSettingsImporter::attempt(const QVariantMap &values,
                                         const QVariantMap &sourceLayers)
{
    if (!d->started || d->writing) return;
    if (values.value(marker).toBool()) {
        d->status = Status::Imported;
        d->message.clear();
        Q_EMIT stateChanged();
        return;
    }
    const auto legacy = d->legacyReader.read();
    if (legacy.outcome == NightLightConfigPort::ReadOutcome::Failed) {
        d->status = Status::InvalidLegacy;
        d->message = legacy.diagnostic.isEmpty()
            ? QStringLiteral("Legacy night light settings are invalid; they were left unchanged.")
            : legacy.diagnostic;
        Q_EMIT stateChanged();
        return;
    }
    d->pending.clear();
    d->values = legacyValues(legacy.values);
    if (legacy.outcome == NightLightConfigPort::ReadOutcome::Loaded) {
        for (const QString &key : importedKeys) {
            if (sourceLayers.value(key).toString() == QLatin1String("user-overrides"))
                continue;
            if (values.value(key) != d->values.value(key))
                d->pending.append(key);
        }
    }
    d->values.insert(marker, true);
    d->pending.append(marker);
    d->index = 0;
    d->status = Status::Importing;
    d->message.clear();
    Q_EMIT stateChanged();
    writeNext();
}

void NightLightSettingsImporter::writeNext()
{
    if (d->index >= d->pending.size()) {
        d->writing = false;
        d->pending.clear();
        d->status = Status::Imported;
        d->message.clear();
        Q_EMIT stateChanged();
        return;
    }
    const QString &key = d->pending.at(d->index);
    QString error;
    if (!d->settings.setUserValue(key, d->values.value(key), &error)) {
        d->writing = false;
        d->status = Status::SaveFailed;
        d->message = error.isEmpty()
            ? QStringLiteral("Legacy night light settings could not be saved.")
            : error;
        d->pending.clear();
        d->index = 0;
        Q_EMIT stateChanged();
        return;
    }
    d->writing = true;
}
} // namespace QindaQt::Services::NightLight
