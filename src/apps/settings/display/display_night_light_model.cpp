// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_display/display_night_light_model.h>

#include <QtCore/QTimeZone>

namespace QindaQt::Apps::SettingsDisplay {

using QindaQt::Services::NightLight::kMaxTransitionSeconds;
using QindaQt::Services::NightLight::kMinTransitionSeconds;
using QindaQt::Services::NightLight::kNeutralTemperatureKelvin;
using QindaQt::Services::NightLight::NightLightSettings;
using QindaQt::Services::NightLight::NightLightStatus;
using ScheduleSettings = QindaQt::Services::NightLight::ScheduleSettings;
using NightLightConfigPort = QindaQt::Services::NightLight::NightLightConfigPort;
using NightLightStatePort = QindaQt::Services::NightLight::NightLightStatePort;
using NightTimeScheduleMonitor =
    QindaQt::Services::NightLight::NightTimeScheduleMonitor;
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
        // Always-on is a KWin mode: the schedule half keeps whatever the
        // machine already holds, and the config port skips unchanged keys.
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
    Private(NightLightConfigPort &config, NightLightStatePort &state,
            NightTimeScheduleMonitor &scheduleMonitor)
        : configPort(config), statePort(state), schedulePort(scheduleMonitor)
    {
    }

    void refreshConfigDraftBase()
    {
        const auto read = configPort.read();
        stored = read.values;
        configLoaded = read.outcome != NightLightConfigPort::ReadOutcome::Failed;
        configFailure =
            read.outcome == NightLightConfigPort::ReadOutcome::Failed
                ? QStringLiteral("Stored night light configuration is invalid.")
                : QString();
        draft = stored;
        draftDirtyValue = false;
    }

    NightLightSettings draftAsSettings() const
    {
        NightLightSettings settings;
        settings.output.active = draft.output.active;
        settings.output.mode = draft.output.mode;
        settings.output.dayTemperatureKelvin =
            draft.output.dayTemperatureKelvin;
        settings.output.nightTemperatureKelvin =
            draft.output.nightTemperatureKelvin;
        settings.schedule = scheduleOf(draftScheduleModeValue, draft.schedule);
        return settings;
    }

    NightLightConfigPort &configPort;
    NightLightStatePort &statePort;
    NightTimeScheduleMonitor &schedulePort;

    NightLightStatus status;
    NightLightSettings stored;
    NightLightSettings draft;
    ScheduleMode draftScheduleModeValue = ScheduleMode::SunsetAuto;
    bool configLoaded = true;
    QString configFailure;
    bool draftDirtyValue = false;
    bool applyingValue = false;
};

DisplayNightLightModel::DisplayNightLightModel(NightLightConfigPort &configPort,
                                               NightLightStatePort &statePort,
                                               NightTimeScheduleMonitor &monitor,
                                               QObject *parent)
    : QObject(parent),
      d(std::make_unique<Private>(configPort, statePort, monitor))
{
    d->refreshConfigDraftBase();
    d->draftScheduleModeValue = scheduleModeOf(d->draft.schedule);

    connect(&d->statePort, &NightLightStatePort::statusChanged, this,
            [this](const NightLightStatus &status) {
                d->status = status;
                Q_EMIT truthChanged();
            });
    connect(&d->statePort, &NightLightStatePort::degraded, this,
            [this](const QString &) { Q_EMIT truthChanged(); });
    connect(&d->schedulePort, &NightTimeScheduleMonitor::
                                  scheduleAvailabilityChanged,
            this, [this](bool scheduleAvailable) {
                Q_EMIT scheduleAvailabilityChanged(scheduleAvailable);
            });
    connect(&d->configPort, &NightLightConfigPort::changedExternally, this,
            [this]() {
                // AGENT-GUARD: An external writer owns the files as much as
                // this model does. Refresh the stored base always, but keep
                // an unsaved user draft intact; the draft stays reviewable
                // against fresh truth instead of being clobbered.
                const bool hadDraft = d->draftDirtyValue;
                const NightLightSettings keptDraft = d->draft;
                const ScheduleMode keptMode = d->draftScheduleModeValue;
                d->refreshConfigDraftBase();
                if (hadDraft) {
                    d->draft = keptDraft;
                    d->draftScheduleModeValue = keptMode;
                    d->draftDirtyValue = true;
                } else {
                    d->draftScheduleModeValue = scheduleModeOf(d->draft.schedule);
                }
                Q_EMIT draftChanged();
                Q_EMIT truthChanged();
            });
}

DisplayNightLightModel::~DisplayNightLightModel() = default;

bool DisplayNightLightModel::available() const
{
    // The unavailable frame and a missing service are the same fail-closed
    // truth for the UI.
    return d->status.available;
}

bool DisplayNightLightModel::scheduleAvailable() const
{
    return d->schedulePort.scheduleAvailable();
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

QString DisplayNightLightModel::statusText() const
{
    if (!available()) {
        return d->configFailure.isEmpty()
                   ? QStringLiteral(
                         "Night light status is unknown right now.")
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
    if (d->applyingValue) {
        return false;
    }
    const NightLightSettings settings = d->draftAsSettings();
    if (!QindaQt::Services::NightLight::isValidOutput(settings.output)
        || !QindaQt::Services::NightLight::isValidSchedule(
            settings.schedule)) {
        Q_EMIT applied(false);
        return false;
    }
    d->applyingValue = true;
    Q_EMIT applyingChanged();
    stopPreview();
    const auto result = d->configPort.write(settings);
    d->applyingValue = false;
    Q_EMIT applyingChanged();
    if (result.outcome == NightLightConfigPort::WriteOutcome::Failed) {
        Q_EMIT applied(false);
        return false;
    }
    // Applied (or already identical): the stored base and the draft agree.
    d->stored = settings;
    d->configLoaded = true;
    d->configFailure.clear();
    d->draftDirtyValue = false;
    Q_EMIT draftChanged();
    Q_EMIT applied(true);
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
