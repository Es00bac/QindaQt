// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QDateTime>
#include <QtCore/QObject>
#include <QtCore/QTime>
#include <QtCore/QVariantList>

namespace QindaQt::Tests::DisplayPageSupport {

// AGENT-CONTRACT: Mirrors the QML surface of DisplayNightLightModel exactly —
// property names, types, invokables, and notification signals — and records
// preview and stopPreview calls instead of touching a bus. Changing
// DisplayNightLightModel's QML surface requires the same change here.
class StubNightLightModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available MEMBER available NOTIFY truthChanged)
    Q_PROPERTY(bool scheduleAvailable MEMBER scheduleAvailable NOTIFY
                   scheduleAvailabilityChanged)
    Q_PROPERTY(bool activeNow MEMBER activeNow NOTIFY truthChanged)
    Q_PROPERTY(bool inhibited MEMBER inhibited NOTIFY truthChanged)
    Q_PROPERTY(int currentTemperatureKelvin MEMBER currentTemperatureKelvin
                   NOTIFY truthChanged)
    Q_PROPERTY(QDateTime nextChangeDateTime MEMBER nextChangeDateTime NOTIFY
                   truthChanged)
    Q_PROPERTY(QString statusText MEMBER statusText NOTIFY truthChanged)
    Q_PROPERTY(bool draftActive MEMBER draftActive NOTIFY draftChanged)
    Q_PROPERTY(int draftScheduleMode MEMBER draftScheduleMode NOTIFY
                   draftChanged)
    Q_PROPERTY(double draftLatitude MEMBER draftLatitude NOTIFY draftChanged)
    Q_PROPERTY(double draftLongitude MEMBER draftLongitude NOTIFY draftChanged)
    Q_PROPERTY(QTime draftSunrise MEMBER draftSunrise NOTIFY draftChanged)
    Q_PROPERTY(QTime draftSunset MEMBER draftSunset NOTIFY draftChanged)
    Q_PROPERTY(int draftTransitionMinutes MEMBER draftTransitionMinutes NOTIFY
                   draftChanged)
    Q_PROPERTY(int draftNightTemperature MEMBER draftNightTemperature NOTIFY
                   draftChanged)
    Q_PROPERTY(int draftDayTemperature MEMBER draftDayTemperature NOTIFY
                   draftChanged)
    Q_PROPERTY(bool draftDirty MEMBER draftDirty NOTIFY draftChanged)
    Q_PROPERTY(QString validationError MEMBER validationError NOTIFY
                   draftChanged)
    Q_PROPERTY(bool applying MEMBER applying NOTIFY applyingChanged)

public:
    explicit StubNightLightModel(QObject *parent = nullptr) : QObject(parent)
    {
    }

    Q_INVOKABLE bool apply()
    {
        ++applyCalls;
        return applyResult;
    }
    Q_INVOKABLE void resetDraft() { ++resetCalls; }
    Q_INVOKABLE void previewTemperature(int kelvin)
    {
        previewTemperatures.append(kelvin);
    }
    Q_INVOKABLE void stopPreview() { ++stopPreviewCalls; }

    bool available = true;
    bool scheduleAvailable = true;
    bool activeNow = false;
    bool inhibited = false;
    int currentTemperatureKelvin = 6500;
    QDateTime nextChangeDateTime = QDateTime(QDate(2026, 9, 11),
                                             QTime(19, 42, 0));
    QString statusText = QStringLiteral("Active — 3400 K now; next change "
                                        "19:42.");

    bool draftActive = true;
    int draftScheduleMode = 0;
    double draftLatitude = 52.5;
    double draftLongitude = 13.25;
    QTime draftSunrise = QTime(6, 30, 0);
    QTime draftSunset = QTime(19, 45, 0);
    int draftTransitionMinutes = 30;
    int draftNightTemperature = 3400;
    int draftDayTemperature = 6500;
    bool draftDirty = false;
    QString validationError;
    bool applying = false;

    int applyCalls = 0;
    int resetCalls = 0;
    int stopPreviewCalls = 0;
    bool applyResult = true;
    QList<int> previewTemperatures;

Q_SIGNALS:
    void truthChanged();
    void scheduleAvailabilityChanged(bool scheduleAvailable);
    void draftChanged();
    void applyingChanged();
    void applied(bool success);
};

} // namespace QindaQt::Tests::DisplayPageSupport
