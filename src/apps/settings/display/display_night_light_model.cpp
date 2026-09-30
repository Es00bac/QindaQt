// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_display/display_night_light_model.h>

#include <QtCore/QVariantMap>
#include <qindaqt/services/settings_protocol/settings_wire_status.h>

namespace QindaQt::Apps::SettingsDisplay {

using QindaQt::Services::NightLight::NightLightSettings;
using QindaQt::Services::NightLight::NightLightStatus;
using ScheduleSettings = QindaQt::Services::NightLight::ScheduleSettings;
using NightLightStatePort = QindaQt::Services::NightLight::NightLightStatePort;
using Mode = QindaQt::Services::NightLight::Mode;
using ScheduleSource = QindaQt::Services::NightLight::ScheduleSource;

namespace {

DisplayNightLightModel::ScheduleMode scheduleModeOf(
    const ScheduleSettings &schedule)
{
    if (schedule.source == ScheduleSource::Times) {
        return DisplayNightLightModel::ScheduleMode::CustomTimes;
    }
    return schedule.automaticLocation
               ? DisplayNightLightModel::ScheduleMode::SunsetAuto
               : DisplayNightLightModel::ScheduleMode::SunsetManual;
}

ScheduleSettings scheduleOf(DisplayNightLightModel::ScheduleMode mode,
                            const ScheduleSettings &base)
{
    ScheduleSettings schedule = base;
    switch (mode) {
    case DisplayNightLightModel::ScheduleMode::SunsetAuto:
        schedule.source = ScheduleSource::Location;
        schedule.automaticLocation = true;
        break;
    case DisplayNightLightModel::ScheduleMode::SunsetManual:
        schedule.source = ScheduleSource::Location;
        schedule.automaticLocation = false;
        break;
    case DisplayNightLightModel::ScheduleMode::CustomTimes:
        schedule.source = ScheduleSource::Times;
        break;
    case DisplayNightLightModel::ScheduleMode::Always:
        // Always-on keeps the selected schedule preferences unchanged.
        break;
    }
    return schedule;
}

QString formatClock(const QDateTime &moment)
{
    if (!moment.isValid()) {
        return QString();
    }
    return QLocale::system().toString(moment.time(), QLocale::ShortFormat);
}

} // namespace

class DisplayNightLightModel::Private {
public:
    Private(QindaQt::Services::SettingsClient::SettingsClient &settings,
            NightLightStatePort &state,
            QindaQt::Services::NightLight::QtNightLightScheduleClient &schedule,
            QindaQt::Services::NightLight::NightLightSettingsImporter &legacyImporter)
        : settingsClient(settings), statePort(state), scheduleClient(schedule), importer(legacyImporter)
    {
    }

    void refreshDraftBase()
    {
        const auto &snapshot = settingsClient.snapshot();
        if (settingsClient.state() != QindaQt::Services::SettingsClient::ClientState::Ready
            || !snapshot) {
            return;
        }
        const QVariantMap &map = snapshot->values;
        stored.output.active = map.value(key("active")).toBool();
        stored.output.mode =
            QindaQt::Services::NightLight::modeFromConfigToken(
                map.value(key("mode")).toString()).value_or(Mode::DarkLight);
        stored.output.dayTemperatureKelvin =
            map.value(key("dayTemperatureKelvin")).toInt();
        stored.output.nightTemperatureKelvin =
            map.value(key("nightTemperatureKelvin")).toInt();
        stored.output.disabledOutputs =
            map.value(key("disabledOutputs")).toStringList();
        stored.schedule.source =
            QindaQt::Services::NightLight::sourceFromConfigToken(
                map.value(key("scheduleSource")).toString())
                .value_or(ScheduleSource::Location);
        stored.schedule.automaticLocation =
            map.value(key("automaticLocation")).toBool();
        stored.schedule.latitudeDegrees =
            map.value(key("latitudeDegrees")).toDouble();
        stored.schedule.longitudeDegrees =
            map.value(key("longitudeDegrees")).toDouble();
        stored.schedule.sunriseStart = QTime::fromString(
            map.value(key("sunriseStart")).toString(), QStringLiteral("HH:mm:ss"));
        stored.schedule.sunsetStart = QTime::fromString(
            map.value(key("sunsetStart")).toString(), QStringLiteral("HH:mm:ss"));
        stored.schedule.transitionSeconds =
            map.value(key("transitionSeconds")).toInt();
        if (!QindaQt::Services::NightLight::isValidOutput(stored.output)
            || !QindaQt::Services::NightLight::isValidSchedule(stored.schedule)) {
            configFailure = QStringLiteral("Saved night light settings are invalid.");
            return;
        }
        configFailure.clear();
        draft = stored;
        draftScheduleModeValue = scheduleModeOf(stored.schedule);
        draftDirtyValue = false;
    }

    NightLightSettings draftAsSettings() const
    {
        NightLightSettings settings = draft;
        settings.schedule = scheduleOf(draftScheduleModeValue, draft.schedule);
        return settings;
    }

    static QString key(const char *suffix)
    {
        return QStringLiteral("display.nightLight.") + QString::fromLatin1(suffix);
    }

    QindaQt::Services::SettingsClient::SettingsClient &settingsClient;
    NightLightStatePort &statePort;
    QindaQt::Services::NightLight::QtNightLightScheduleClient &scheduleClient;
    QindaQt::Services::NightLight::NightLightSettingsImporter &importer;
    NightLightStatus status;
    NightLightSettings stored;
    NightLightSettings draft;
    ScheduleMode draftScheduleModeValue = ScheduleMode::SunsetAuto;
    QString configFailure;
    QStringList pendingKeys;
    QVariantMap pendingValues;
    qsizetype pendingIndex = 0;
    bool draftDirtyValue = false;
    bool applyingValue = false;
};

DisplayNightLightModel::DisplayNightLightModel(
    QindaQt::Services::SettingsClient::SettingsClient &settingsClient,
    NightLightStatePort &statePort,
    QindaQt::Services::NightLight::QtNightLightScheduleClient &scheduleClient,
    QindaQt::Services::NightLight::NightLightSettingsImporter &importer,
    QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>(settingsClient, statePort, scheduleClient, importer))
{
    d->refreshDraftBase();
    connect(&d->statePort, &NightLightStatePort::statusChanged, this,
            [this](const NightLightStatus &status) {
                d->status = status;
                Q_EMIT truthChanged();
            });
    connect(&d->statePort, &NightLightStatePort::degraded, this,
            [this](const QString &) { Q_EMIT truthChanged(); });
    connect(&d->scheduleClient,
            &QindaQt::Services::NightLight::QtNightLightScheduleClient::stateChanged,
            this, [this] {
                Q_EMIT scheduleAvailabilityChanged(scheduleAvailable());
                Q_EMIT truthChanged();
            });
    connect(&d->importer, &QindaQt::Services::NightLight::NightLightSettingsImporter::stateChanged,
            this, &DisplayNightLightModel::truthChanged);
    connect(&d->settingsClient, &QindaQt::Services::SettingsClient::SettingsClient::snapshotChanged,
            this, [this] {
                const bool keepDraft = d->draftDirtyValue;
                const NightLightSettings keptDraft = d->draft;
                const ScheduleMode keptMode = d->draftScheduleModeValue;
                d->refreshDraftBase();
                if (keepDraft) {
                    d->draft = keptDraft;
                    d->draftScheduleModeValue = keptMode;
                    d->draftDirtyValue = true;
                }
                Q_EMIT draftChanged();
                Q_EMIT truthChanged();
            });
    connect(&d->settingsClient, &QindaQt::Services::SettingsClient::SettingsClient::stateChanged,
            this, [this] {
                if (d->settingsClient.state()
                    == QindaQt::Services::SettingsClient::ClientState::Ready
                    && !d->draftDirtyValue) {
                    d->refreshDraftBase();
                    Q_EMIT draftChanged();
                }
                Q_EMIT truthChanged();
            });
    connect(&d->settingsClient,
            &QindaQt::Services::SettingsClient::SettingsClient::commitFinished,
            this, [this](const QindaQt::Services::SettingsClient::CommitOutcome &outcome) {
                using QindaQt::Services::SettingsProtocol::SettingsWireStatus;
                if (!d->applyingValue) return;
                if (outcome.status != SettingsWireStatus::Applied) {
                    d->applyingValue = false;
                    d->configFailure = outcome.message.isEmpty()
                        ? QStringLiteral("Night light settings could not be saved.")
                        : outcome.message;
                    Q_EMIT applyingChanged();
                    Q_EMIT truthChanged();
                    Q_EMIT applied(false);
                    return;
                }
                ++d->pendingIndex;
                if (d->pendingIndex < d->pendingKeys.size()) {
                    QString error;
                    const QString &nextKey = d->pendingKeys.at(d->pendingIndex);
                    if (!d->settingsClient.setUserValue(nextKey,
                            d->pendingValues.value(nextKey), &error)) {
                        d->applyingValue = false;
                        d->configFailure = error;
                        Q_EMIT applyingChanged();
                        Q_EMIT truthChanged();
                        Q_EMIT applied(false);
                    }
                    return;
                }
                d->applyingValue = false;
                d->pendingKeys.clear();
                d->pendingValues.clear();
                d->pendingIndex = 0;
                d->stored = d->draftAsSettings();
                d->draft = d->stored;
                d->draftDirtyValue = false;
                d->configFailure.clear();
                Q_EMIT applyingChanged();
                Q_EMIT draftChanged();
                Q_EMIT applied(true);
            });
    connect(&d->settingsClient,
            &QindaQt::Services::SettingsClient::SettingsClient::commitUncertain,
            this, [this](const QString &message) {
                if (!d->applyingValue) return;
                d->applyingValue = false;
                d->pendingKeys.clear();
                d->pendingValues.clear();
                d->pendingIndex = 0;
                d->configFailure = message;
                Q_EMIT applyingChanged();
                Q_EMIT truthChanged();
                Q_EMIT applied(false);
            });
}

DisplayNightLightModel::~DisplayNightLightModel() = default;

bool DisplayNightLightModel::available() const
{
    // The unavailable frame and a missing service are the same fail-closed
    // truth for the UI.
    return d->status.available
        && d->settingsClient.state() == QindaQt::Services::SettingsClient::ClientState::Ready;
}

bool DisplayNightLightModel::scheduleAvailable() const
{
    const auto &state = d->scheduleClient.state();
    return state && state->schedule.available;
}

bool DisplayNightLightModel::activeNow() const
{
    return d->status.enabled && d->status.running;
}

bool DisplayNightLightModel::inhibited() const
{
    return d->status.inhibited;
}

int DisplayNightLightModel::currentTemperatureKelvin() const
{
    return d->status.currentTemperatureKelvin;
}

QDateTime DisplayNightLightModel::nextChangeDateTime() const
{
    return d->status.scheduledTransition.dateTime.toLocalTime();
}

QString DisplayNightLightModel::migrationMessage() const
{
    return d->importer.message();
}

bool DisplayNightLightModel::migrationRetryable() const
{
    using Status = QindaQt::Services::NightLight::NightLightSettingsImporter::Status;
    const auto state = d->importer.status();
    return state == Status::InvalidLegacy || state == Status::SaveFailed
        || state == Status::RetryRequired;
}

void DisplayNightLightModel::retryMigration()
{
    d->importer.retry();
}

QString DisplayNightLightModel::statusText() const
{
    if (!available()) {
        return d->configFailure.isEmpty()
                   ? QStringLiteral("Night light status is unknown right now.")
                   : d->configFailure;
    }
    const int current = d->status.currentTemperatureKelvin;
    const QString next = formatClock(nextChangeDateTime());
    if (d->status.inhibited) {
        return QStringLiteral("Paused — %1 K now.").arg(current);
    }
    if (d->status.enabled && d->status.running) {
        return next.isEmpty()
                   ? QStringLiteral("Active — %1 K now.").arg(current)
                   : QStringLiteral("Active — %1 K now; next change %2.")
                         .arg(current).arg(next);
    }
    if (d->status.enabled) {
        return QStringLiteral("On — %1 K now; waiting for the schedule.")
            .arg(current);
    }
    return QStringLiteral("Off — %1 K.").arg(current);
}

bool DisplayNightLightModel::draftActive() const
{
    return d->draft.output.active;
}

void DisplayNightLightModel::setDraftActive(bool active)
{
    if (d->draft.output.active == active) {
        return;
    }
    d->draft.output.active = active;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

DisplayNightLightModel::ScheduleMode
DisplayNightLightModel::draftScheduleMode() const
{
    return d->draftScheduleModeValue;
}

void DisplayNightLightModel::setDraftScheduleMode(ScheduleMode mode)
{
    if (d->draftScheduleModeValue == mode) {
        return;
    }
    d->draftScheduleModeValue = mode;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

double DisplayNightLightModel::draftLatitude() const
{
    return d->draft.schedule.latitudeDegrees;
}

void DisplayNightLightModel::setDraftLatitude(double latitude)
{
    if (d->draft.schedule.latitudeDegrees == latitude) {
        return;
    }
    d->draft.schedule.latitudeDegrees = latitude;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

double DisplayNightLightModel::draftLongitude() const
{
    return d->draft.schedule.longitudeDegrees;
}

void DisplayNightLightModel::setDraftLongitude(double longitude)
{
    if (d->draft.schedule.longitudeDegrees == longitude) {
        return;
    }
    d->draft.schedule.longitudeDegrees = longitude;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

QTime DisplayNightLightModel::draftSunrise() const
{
    return d->draft.schedule.sunriseStart;
}

void DisplayNightLightModel::setDraftSunrise(QTime sunrise)
{
    if (d->draft.schedule.sunriseStart == sunrise) {
        return;
    }
    d->draft.schedule.sunriseStart = sunrise;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

QTime DisplayNightLightModel::draftSunset() const
{
    return d->draft.schedule.sunsetStart;
}

void DisplayNightLightModel::setDraftSunset(QTime sunset)
{
    if (d->draft.schedule.sunsetStart == sunset) {
        return;
    }
    d->draft.schedule.sunsetStart = sunset;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

int DisplayNightLightModel::draftTransitionMinutes() const
{
    return d->draft.schedule.transitionSeconds / 60;
}

void DisplayNightLightModel::setDraftTransitionMinutes(int minutes)
{
    const int seconds = minutes * 60;
    if (d->draft.schedule.transitionSeconds == seconds) {
        return;
    }
    d->draft.schedule.transitionSeconds = seconds;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

int DisplayNightLightModel::draftNightTemperature() const
{
    return d->draft.output.nightTemperatureKelvin;
}

void DisplayNightLightModel::setDraftNightTemperature(int kelvin)
{
    kelvin = QindaQt::Services::NightLight::snapTemperature(kelvin);
    if (d->draft.output.nightTemperatureKelvin == kelvin) {
        return;
    }
    d->draft.output.nightTemperatureKelvin = kelvin;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

int DisplayNightLightModel::draftDayTemperature() const
{
    return d->draft.output.dayTemperatureKelvin;
}

void DisplayNightLightModel::setDraftDayTemperature(int kelvin)
{
    kelvin = QindaQt::Services::NightLight::snapTemperature(kelvin);
    if (d->draft.output.dayTemperatureKelvin == kelvin) {
        return;
    }
    d->draft.output.dayTemperatureKelvin = kelvin;
    d->draftDirtyValue = true;
    Q_EMIT draftChanged();
}

bool DisplayNightLightModel::draftDirty() const
{
    return d->draftDirtyValue;
}

QString DisplayNightLightModel::validationError() const
{
    QString error;
    const NightLightSettings settings = d->draftAsSettings();
    if (!QindaQt::Services::NightLight::isValidOutput(settings.output)) {
        error = QStringLiteral("Temperatures must be between %1 K and %2 K.")
                    .arg(QindaQt::Services::NightLight::kTemperatureFloorKelvin)
                    .arg(QindaQt::Services::NightLight::kTemperatureCeilingKelvin);
    } else if (!QindaQt::Services::NightLight::isValidSchedule(
                   settings.schedule)) {
        error = QStringLiteral(
                    "Check the coordinates, times, and transition length.");
    }
    return error;
}

bool DisplayNightLightModel::applying() const
{
    return d->applyingValue;
}

bool DisplayNightLightModel::apply()
{
    if (d->applyingValue
        || d->settingsClient.state()
            != QindaQt::Services::SettingsClient::ClientState::Ready
        || !d->settingsClient.snapshot()) {
        Q_EMIT applied(false);
        return false;
    }
    const NightLightSettings settings = d->draftAsSettings();
    if (!QindaQt::Services::NightLight::isValidOutput(settings.output)
        || !QindaQt::Services::NightLight::isValidSchedule(settings.schedule)) {
        Q_EMIT applied(false);
        return false;
    }
    const QVariantMap values{
        {Private::key("active"), settings.output.active},
        {Private::key("mode"), QindaQt::Services::NightLight::modeToConfigToken(settings.output.mode)},
        {Private::key("dayTemperatureKelvin"), settings.output.dayTemperatureKelvin},
        {Private::key("nightTemperatureKelvin"), settings.output.nightTemperatureKelvin},
        {Private::key("scheduleSource"), QindaQt::Services::NightLight::sourceToConfigToken(settings.schedule.source)},
        {Private::key("automaticLocation"), settings.schedule.automaticLocation},
        {Private::key("latitudeDegrees"), settings.schedule.latitudeDegrees},
        {Private::key("longitudeDegrees"), settings.schedule.longitudeDegrees},
        {Private::key("sunriseStart"), settings.schedule.sunriseStart.toString(QStringLiteral("HH:mm:ss"))},
        {Private::key("sunsetStart"), settings.schedule.sunsetStart.toString(QStringLiteral("HH:mm:ss"))},
        {Private::key("transitionSeconds"), settings.schedule.transitionSeconds},
        {Private::key("disabledOutputs"), settings.output.disabledOutputs}};
    d->pendingKeys.clear();
    d->pendingValues = values;
    const QVariantMap &current = d->settingsClient.snapshot()->values;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        if (current.value(it.key()) != it.value()) d->pendingKeys.append(it.key());
    }
    if (d->pendingKeys.isEmpty()) {
        d->stored = settings;
        d->draftDirtyValue = false;
        Q_EMIT draftChanged();
        Q_EMIT applied(true);
        return true;
    }
    d->applyingValue = true;
    d->pendingIndex = 0;
    Q_EMIT applyingChanged();
    stopPreview();
    QString error;
    const QString &first = d->pendingKeys.first();
    if (!d->settingsClient.setUserValue(first, d->pendingValues.value(first), &error)) {
        d->applyingValue = false;
        d->pendingKeys.clear();
        d->pendingValues.clear();
        d->pendingIndex = 0;
        d->configFailure = error;
        Q_EMIT applyingChanged();
        Q_EMIT truthChanged();
        Q_EMIT applied(false);
        return false;
    }
    return true;
}

void DisplayNightLightModel::resetDraft()
{
    d->draft = d->stored;
    d->draftScheduleModeValue = scheduleModeOf(d->draft.schedule);
    d->draftDirtyValue = false;
    Q_EMIT draftChanged();
}

void DisplayNightLightModel::previewTemperature(int kelvin)
{
    if (!available() || !d->draft.output.active) {
        return;
    }
    d->statePort.preview(kelvin);
}

void DisplayNightLightModel::stopPreview()
{
    d->statePort.stopPreview();
}

} // namespace QindaQt::Apps::SettingsDisplay
