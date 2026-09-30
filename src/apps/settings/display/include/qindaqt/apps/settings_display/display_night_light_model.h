// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <qindaqt/services/night_light/night_light_schedule_client.h>
#include <qindaqt/services/night_light/night_light_state_port.h>
#include <qindaqt/services/night_light/night_light_settings_importer.h>
#include <qindaqt/services/settings_client/settings_client.h>

#include <QtCore/QObject>
#include <QtCore/QDateTime>
#include <QtCore/QTime>

#include <memory>

namespace QindaQt::Apps::SettingsDisplay {

// Settings1-backed Display model. Live output truth remains on KWin's public
// NightLight boundary; schedule health comes from the resident Schedule1 client.
// Writes are asynchronous and the model reports success only after every
// per-key commit has been acknowledged.

class DisplayNightLightModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY truthChanged)
    Q_PROPERTY(bool scheduleAvailable READ scheduleAvailable NOTIFY
                   scheduleAvailabilityChanged)
    // Live truth from the KWin service.
    Q_PROPERTY(bool activeNow READ activeNow NOTIFY truthChanged)
    Q_PROPERTY(bool inhibited READ inhibited NOTIFY truthChanged)
    Q_PROPERTY(int currentTemperatureKelvin READ currentTemperatureKelvin
                   NOTIFY truthChanged)
    Q_PROPERTY(QDateTime nextChangeDateTime READ nextChangeDateTime NOTIFY
                   truthChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY truthChanged)
    Q_PROPERTY(QString migrationMessage READ migrationMessage NOTIFY truthChanged)
    Q_PROPERTY(bool migrationRetryable READ migrationRetryable NOTIFY truthChanged)
    // Draft (user intent until apply()).
    Q_PROPERTY(bool draftActive READ draftActive WRITE setDraftActive NOTIFY
                   draftChanged)
    Q_PROPERTY(ScheduleMode draftScheduleMode READ draftScheduleMode WRITE
                   setDraftScheduleMode NOTIFY draftChanged)
    Q_PROPERTY(double draftLatitude READ draftLatitude WRITE setDraftLatitude
                   NOTIFY draftChanged)
    Q_PROPERTY(double draftLongitude READ draftLongitude WRITE
                   setDraftLongitude NOTIFY draftChanged)
    Q_PROPERTY(QTime draftSunrise READ draftSunrise WRITE setDraftSunrise
                   NOTIFY draftChanged)
    Q_PROPERTY(QTime draftSunset READ draftSunset WRITE setDraftSunset NOTIFY
                   draftChanged)
    Q_PROPERTY(int draftTransitionMinutes READ draftTransitionMinutes WRITE
                   setDraftTransitionMinutes NOTIFY draftChanged)
    Q_PROPERTY(int draftNightTemperature READ draftNightTemperature WRITE
                   setDraftNightTemperature NOTIFY draftChanged)
    Q_PROPERTY(int draftDayTemperature READ draftDayTemperature WRITE
                   setDraftDayTemperature NOTIFY draftChanged)
    Q_PROPERTY(bool draftDirty READ draftDirty NOTIFY draftChanged)
    Q_PROPERTY(QString validationError READ validationError NOTIFY draftChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY applyingChanged)

public:
    enum class ScheduleMode {
        SunsetAuto,
        SunsetManual,
        CustomTimes,
        Always,
    };
    Q_ENUM(ScheduleMode)

    explicit DisplayNightLightModel(
        QindaQt::Services::SettingsClient::SettingsClient &settingsClient,
        QindaQt::Services::NightLight::NightLightStatePort &statePort,
        QindaQt::Services::NightLight::QtNightLightScheduleClient &scheduleClient,
        QindaQt::Services::NightLight::NightLightSettingsImporter &importer,
        QObject *parent = nullptr);
    ~DisplayNightLightModel() override;

    [[nodiscard]] bool available() const;
    [[nodiscard]] bool scheduleAvailable() const;
    [[nodiscard]] bool activeNow() const;
    [[nodiscard]] bool inhibited() const;
    [[nodiscard]] int currentTemperatureKelvin() const;
    [[nodiscard]] QDateTime nextChangeDateTime() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString migrationMessage() const;
    [[nodiscard]] bool migrationRetryable() const;
    Q_INVOKABLE void retryMigration();

    [[nodiscard]] bool draftActive() const;
    void setDraftActive(bool active);
    [[nodiscard]] ScheduleMode draftScheduleMode() const;
    void setDraftScheduleMode(ScheduleMode mode);
    [[nodiscard]] double draftLatitude() const;
    void setDraftLatitude(double latitude);
    [[nodiscard]] double draftLongitude() const;
    void setDraftLongitude(double longitude);
    [[nodiscard]] QTime draftSunrise() const;
    void setDraftSunrise(QTime sunrise);
    [[nodiscard]] QTime draftSunset() const;
    void setDraftSunset(QTime sunset);
    [[nodiscard]] int draftTransitionMinutes() const;
    void setDraftTransitionMinutes(int minutes);
    [[nodiscard]] int draftNightTemperature() const;
    void setDraftNightTemperature(int kelvin);
    [[nodiscard]] int draftDayTemperature() const;
    void setDraftDayTemperature(int kelvin);
    [[nodiscard]] bool draftDirty() const;
    [[nodiscard]] QString validationError() const;
    [[nodiscard]] bool applying() const;

    // Validates the draft and writes changed Settings1 keys asynchronously.
    // The applied signal follows authoritative replies for every write.
    Q_INVOKABLE bool apply();
    // Reverts the draft to the last applied (or externally changed) values.
    Q_INVOKABLE void resetDraft();
    // KWin's short preview; the section debounces so one call settles per
    // drag. Out-of-range values are refused by the port.
    Q_INVOKABLE void previewTemperature(int kelvin);
    Q_INVOKABLE void stopPreview();

Q_SIGNALS:
    void truthChanged();
    void scheduleAvailabilityChanged(bool scheduleAvailable);
    void draftChanged();
    void applyingChanged();
    void applied(bool success);

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace QindaQt::Apps::SettingsDisplay
