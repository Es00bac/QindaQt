// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/services/night_light/night_light_config_port.h>

#include <KConfig>
#include <KConfigGroup>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>

#include <optional>
#include <utility>

namespace QindaQt::Services::NightLight {
namespace {

// kwinrc group and keys, per the pinned KWin's nightlightsettings.kcfg.
const QString kKwinGroup = QStringLiteral("NightColor");
const QString kActiveKey = QStringLiteral("Active");
const QString kModeKey = QStringLiteral("Mode");
const QString kDayTemperatureKey = QStringLiteral("DayTemperature");
const QString kNightTemperatureKey = QStringLiteral("NightTemperature");

// knighttimerc groups and keys, per upstream knighttime v6.6.6's
// kdarklightsettings.kcfg. Enum entries persist as choice names and the
// transition duration is seconds (ADR-0136).
const QString kGeneralGroup = QStringLiteral("General");
const QString kSourceKey = QStringLiteral("Source");
const QString kLocationGroup = QStringLiteral("Location");
const QString kAutomaticKey = QStringLiteral("Automatic");
const QString kLatitudeKey = QStringLiteral("Latitude");
const QString kLongitudeKey = QStringLiteral("Longitude");
const QString kTimesGroup = QStringLiteral("Times");
const QString kSunriseStartKey = QStringLiteral("SunriseStart");
const QString kSunsetStartKey = QStringLiteral("SunsetStart");
const QString kTransitionDurationKey = QStringLiteral("TransitionDuration");

// AGENT-CONTRACT: Values are read as raw strings and parsed here, not through
// KConfig's typed fallbacks: a malformed stored entry ("abc" for a
// temperature, a legacy "0630" time) must fail the whole read, while a typed
// readEntry would silently swallow it into the default.
std::optional<bool> parseBool(const QString &raw)
{
    if (raw == QLatin1String("true") || raw == QLatin1String("1")) {
        return true;
    }
    if (raw == QLatin1String("false") || raw == QLatin1String("0")) {
        return false;
    }
    return std::nullopt;
}

std::optional<int> parseInt(const QString &raw)
{
    bool ok = false;
    const int parsed = raw.toInt(&ok);
    return ok ? std::optional<int>(parsed) : std::nullopt;
}

std::optional<double> parseDouble(const QString &raw)
{
    bool ok = false;
    const double parsed = raw.toDouble(&ok);
    return ok ? std::optional<double>(parsed) : std::nullopt;
}

std::optional<QTime> parseTime(const QString &raw)
{
    const QTime parsed = QTime::fromString(raw, QStringLiteral("HH:mm:ss"));
    return parsed.isValid() ? std::optional<QTime>(parsed) : std::nullopt;
}

// Modification identity used to recognize our own write echoes: a real
// external edit always lands with a fresh modification time.
QString fileIdentity(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return QStringLiteral("absent");
    }
    return QStringLiteral("%1:%2").arg(info.lastModified().toMSecsSinceEpoch())
        .arg(info.size());
}

} // namespace

class QtConfigNightLightPort::Private {
public:
    Private(QString kwinRcPath, QString knightTimeRcPath)
        : kwinRc(std::move(kwinRcPath)),
          knightTimeRc(std::move(knightTimeRcPath))
    {
    }

    QString kwinRc;
    QString knightTimeRc;

    // AGENT-GUARD: QFileSystemWatcher delivers its signals asynchronously, so
    // own-write echoes arrive after write() returned. Suppression therefore
    // compares the observed file identities against the identities recorded
    // by the last write; a real external edit produces a different identity
    // and is reported as changedExternally(). Clearing these snapshots turns
    // every subsequent event into an external change, which is the safe
    // default.
    QString lastWriteKwinIdentity = QStringLiteral("none");
    QString lastWriteKnightIdentity = QStringLiteral("none");
};

NightLightConfigPort::NightLightConfigPort(QObject *parent)
    : QObject(parent)
{
}

QtConfigNightLightPort::QtConfigNightLightPort(QString kwinRcPath,
                                               QString knightTimeRcPath,
                                               QObject *parent)
    : NightLightConfigPort(parent),
      d(std::make_unique<Private>(std::move(kwinRcPath),
                                  std::move(knightTimeRcPath)))
{
    auto *watcher = new QFileSystemWatcher(this);
    // AGENT-NOTE: The watched files may not exist yet (fresh profile), so the
    // port watches their parent directories as well and re-arms file watches
    // when a directory changes.
    const QStringList directories = {
        QFileInfo(d->kwinRc).absolutePath(),
        QFileInfo(d->knightTimeRc).absolutePath(),
    };
    const auto reportIfExternal = [this]() {
        if (fileIdentity(d->kwinRc) == d->lastWriteKwinIdentity
            && fileIdentity(d->knightTimeRc) == d->lastWriteKnightIdentity) {
            return;
        }
        d->lastWriteKwinIdentity = QStringLiteral("none");
        d->lastWriteKnightIdentity = QStringLiteral("none");
        Q_EMIT changedExternally();
    };
    connect(watcher, &QFileSystemWatcher::directoryChanged, this,
            [this, watcher, reportIfExternal](const QString &) {
                const QStringList files = { d->kwinRc, d->knightTimeRc };
                for (const QString &file : files) {
                    if (QFileInfo::exists(file)
                        && !watcher->files().contains(file)) {
                        watcher->addPath(file);
                    }
                }
                reportIfExternal();
            });
    connect(watcher, &QFileSystemWatcher::fileChanged, this,
            [this, watcher, reportIfExternal](const QString &file) {
                // A replaced file drops its watch on Linux; re-arm so later
                // external edits stay observable.
                if (QFileInfo::exists(file)
                    && !watcher->files().contains(file)) {
                    watcher->addPath(file);
                }
                reportIfExternal();
            });
    watcher->addPaths(directories);
    const QStringList candidateFiles = { d->kwinRc, d->knightTimeRc };
    QStringList presentFiles;
    for (const QString &file : candidateFiles) {
        if (QFileInfo::exists(file)) {
            presentFiles.append(file);
        }
    }
    if (!presentFiles.isEmpty()) {
        watcher->addPaths(presentFiles);
    }
}

QtConfigNightLightPort::~QtConfigNightLightPort() = default;

NightLightConfigPort::ReadResult QtConfigNightLightPort::read() const
{
    ReadResult result;

    // SimpleConfig: exactly the injected files — no cascading from system
    // config directories, no kdeglobals inclusion.
    KConfig kwin(d->kwinRc, KConfig::SimpleConfig);
    KConfigGroup kwinGroup(&kwin, kKwinGroup);
    const auto active =
        kwinGroup.hasKey(kActiveKey)
            ? parseBool(kwinGroup.readEntry(kActiveKey, QString()))
            : std::nullopt;
    const auto modeToken =
        kwinGroup.hasKey(kModeKey)
            ? std::optional<QString>(kwinGroup.readEntry(kModeKey, QString()))
            : std::nullopt;
    const auto dayKelvin =
        kwinGroup.hasKey(kDayTemperatureKey)
            ? parseInt(kwinGroup.readEntry(kDayTemperatureKey, QString()))
            : std::nullopt;
    const auto nightKelvin =
        kwinGroup.hasKey(kNightTemperatureKey)
            ? parseInt(kwinGroup.readEntry(kNightTemperatureKey, QString()))
            : std::nullopt;

    KConfig knight(d->knightTimeRc, KConfig::SimpleConfig);
    KConfigGroup generalGroup(&knight, kGeneralGroup);
    const auto sourceToken =
        generalGroup.hasKey(kSourceKey)
            ? std::optional<QString>(generalGroup.readEntry(kSourceKey,
                                                            QString()))
            : std::nullopt;
    KConfigGroup locationGroup(&knight, kLocationGroup);
    const auto automatic =
        locationGroup.hasKey(kAutomaticKey)
            ? parseBool(locationGroup.readEntry(kAutomaticKey, QString()))
            : std::nullopt;
    const auto latitude =
        locationGroup.hasKey(kLatitudeKey)
            ? parseDouble(locationGroup.readEntry(kLatitudeKey, QString()))
            : std::nullopt;
    const auto longitude =
        locationGroup.hasKey(kLongitudeKey)
            ? parseDouble(locationGroup.readEntry(kLongitudeKey, QString()))
            : std::nullopt;
    KConfigGroup timesGroup(&knight, kTimesGroup);
    const auto sunrise =
        timesGroup.hasKey(kSunriseStartKey)
            ? parseTime(timesGroup.readEntry(kSunriseStartKey, QString()))
            : std::nullopt;
    const auto sunset =
        timesGroup.hasKey(kSunsetStartKey)
            ? parseTime(timesGroup.readEntry(kSunsetStartKey, QString()))
            : std::nullopt;
    const auto transition =
        timesGroup.hasKey(kTransitionDurationKey)
            ? parseInt(timesGroup.readEntry(kTransitionDurationKey, QString()))
            : std::nullopt;

    const bool anyPresent =
        active.has_value() || modeToken.has_value() || dayKelvin.has_value()
        || nightKelvin.has_value() || sourceToken.has_value()
        || automatic.has_value() || latitude.has_value()
        || longitude.has_value() || sunrise.has_value() || sunset.has_value()
        || transition.has_value();
    result.outcome = anyPresent ? ReadOutcome::Loaded : ReadOutcome::Absent;

    // A stored token must decode through the kcfg vocabulary; an unknown
    // token leaves the parse empty and fails the whole read below.
    const std::optional<Mode> parsedMode =
        modeToken.has_value() ? modeFromConfigToken(*modeToken)
                              : std::optional<Mode>(Mode::DarkLight);
    const std::optional<ScheduleSource> parsedSource =
        sourceToken.has_value() ? sourceFromConfigToken(*sourceToken)
                                : std::optional<ScheduleSource>(
                                    ScheduleSource::Location);

    result.values.output.active = active.value_or(false);
    result.values.output.mode = parsedMode.value_or(Mode::DarkLight);
    result.values.schedule.source =
        parsedSource.value_or(ScheduleSource::Location);
    result.values.schedule.automaticLocation = automatic.value_or(true);
    result.values.schedule.latitudeDegrees = latitude.value_or(0.0);
    result.values.schedule.longitudeDegrees = longitude.value_or(0.0);
    result.values.schedule.sunriseStart = sunrise.value_or(QTime(6, 0, 0));
    result.values.schedule.sunsetStart = sunset.value_or(QTime(18, 0, 0));

    if (dayKelvin.has_value()) {
        result.values.output.dayTemperatureKelvin = *dayKelvin;
    }
    if (nightKelvin.has_value()) {
        result.values.output.nightTemperatureKelvin = *nightKelvin;
    }
    if (transition.has_value()) {
        result.values.schedule.transitionSeconds = *transition;
    }

    if (!parsedMode.has_value() || !parsedSource.has_value()
        || !isValidOutput(result.values.output)
        || !isValidSchedule(result.values.schedule)) {
        result.outcome = ReadOutcome::Failed;
        result.diagnostic = QStringLiteral(
            "night light configuration holds an unknown token or an "
            "out-of-bounds value");
    }
    return result;
}

NightLightConfigPort::WriteResult
QtConfigNightLightPort::write(const NightLightSettings &settings)
{
    if (!isValidOutput(settings.output)
        || !isValidSchedule(settings.schedule)) {
        return { WriteOutcome::Failed,
                 QStringLiteral("refusing to write an invalid night light "
                                "configuration") };
    }

    const NightLightConfigPort::ReadResult current = read();
    if (current.outcome != ReadOutcome::Failed
        && current.values == settings) {
        return { WriteOutcome::Unchanged, QString() };
    }
    // AGENT-GUARD: On a hostile stored value (ReadOutcome::Failed) the port
    // deliberately rewrites every owned key with validated values instead of
    // merging: keeping any part of the hostile state, or copying it into the
    // rewrite, would publish lies to KWin and knighttimed. In every other
    // case only keys whose desired value differs are touched, so an external
    // writer's formatting of unchanged keys survives.

    // kwinrc first (live-output truth), then knighttimerc (schedule truth).
    // A failed second file leaves a Failed result; the next write converges
    // because both files are re-read on every call.
    if (!QDir().mkpath(QFileInfo(d->kwinRc).absolutePath())) {
        return { WriteOutcome::Failed,
                 QStringLiteral("kwinrc directory is not writable") };
    }
    if (!QDir().mkpath(QFileInfo(d->knightTimeRc).absolutePath())) {
        return { WriteOutcome::Failed,
                 QStringLiteral("knighttimerc directory is not writable") };
    }

    KConfig kwin(d->kwinRc, KConfig::SimpleConfig);
    KConfigGroup kwinGroup(&kwin, kKwinGroup);
    if (current.outcome == ReadOutcome::Failed
        || current.values.output.active != settings.output.active) {
        kwinGroup.writeEntry(kActiveKey, settings.output.active, KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.output.mode != settings.output.mode) {
        // Enum names are written as the exact kcfg choice-name strings.
        kwinGroup.writeEntry(kModeKey, modeToConfigToken(settings.output.mode), KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.output.dayTemperatureKelvin
               != settings.output.dayTemperatureKelvin) {
        kwinGroup.writeEntry(kDayTemperatureKey,
                             settings.output.dayTemperatureKelvin, KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.output.nightTemperatureKelvin
               != settings.output.nightTemperatureKelvin) {
        kwinGroup.writeEntry(kNightTemperatureKey,
                             settings.output.nightTemperatureKelvin, KConfigBase::Notify);
    }
    if (!kwin.sync()) {
        return { WriteOutcome::Failed, QStringLiteral("kwinrc write failed") };
    }

    KConfig knight(d->knightTimeRc, KConfig::SimpleConfig);
    KConfigGroup generalGroup(&knight, kGeneralGroup);
    KConfigGroup locationGroup(&knight, kLocationGroup);
    KConfigGroup timesGroup(&knight, kTimesGroup);
    if (current.outcome == ReadOutcome::Failed
        || current.values.schedule.source != settings.schedule.source) {
        generalGroup.writeEntry(kSourceKey,
                                sourceToConfigToken(settings.schedule.source), KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.schedule.automaticLocation
               != settings.schedule.automaticLocation) {
        locationGroup.writeEntry(kAutomaticKey,
                                 settings.schedule.automaticLocation, KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.schedule.latitudeDegrees
               != settings.schedule.latitudeDegrees) {
        locationGroup.writeEntry(kLatitudeKey,
                                 settings.schedule.latitudeDegrees, KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.schedule.longitudeDegrees
               != settings.schedule.longitudeDegrees) {
        locationGroup.writeEntry(kLongitudeKey,
                                 settings.schedule.longitudeDegrees, KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.schedule.sunriseStart
               != settings.schedule.sunriseStart) {
        timesGroup.writeEntry(kSunriseStartKey, settings.schedule.sunriseStart, KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.schedule.sunsetStart
               != settings.schedule.sunsetStart) {
        timesGroup.writeEntry(kSunsetStartKey, settings.schedule.sunsetStart, KConfigBase::Notify);
    }
    if (current.outcome == ReadOutcome::Failed
        || current.values.schedule.transitionSeconds
               != settings.schedule.transitionSeconds) {
        timesGroup.writeEntry(kTransitionDurationKey,
                              settings.schedule.transitionSeconds, KConfigBase::Notify);
    }
    if (!knight.sync()) {
        return { WriteOutcome::Failed,
                 QStringLiteral("knighttimerc write failed") };
    }

    d->lastWriteKwinIdentity = fileIdentity(d->kwinRc);
    d->lastWriteKnightIdentity = fileIdentity(d->knightTimeRc);
    return { WriteOutcome::Applied, QString() };
}

} // namespace QindaQt::Services::NightLight
