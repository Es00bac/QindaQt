// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// The Settings Audio stub's fixture data, held apart from the stub itself so
// the stub reads as what it is - a mirror of the real model's surface - rather
// than as a wall of sample rows.

#include <QtCore/QStringList>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsAudio::TestSupport
{

inline QVariantList channelRows(const QStringList &positions,
                                const int volumePercent) {
  QVariantList rows;
  for (int index = 0; index < positions.size(); ++index) {
    rows.append(QVariantMap{
        {QStringLiteral("index"), index},
        {QStringLiteral("position"), positions.at(index)},
        {QStringLiteral("volumePercent"), volumePercent},
        {QStringLiteral("level01"), volumePercent / 100.0},
    });
  }
  return rows;
}

inline QVariantList makeOutputDevices()
{
    return {
        QVariantMap{
            {QStringLiteral("serial"), qulonglong(10)},
            {QStringLiteral("kindText"), QStringLiteral("Output device")},
            {QStringLiteral("displayName"), QStringLiteral("Desk Speakers")},
            {QStringLiteral("volumePercent"), 50},
            {QStringLiteral("volumeKnown"), true},
            {QStringLiteral("muted"), false},
            {QStringLiteral("muteKnown"), true},
            {QStringLiteral("isDefault"), true},
            {QStringLiteral("setDefaultAvailable"), true},
            {QStringLiteral("volumeAvailable"), true},
            {QStringLiteral("muteAvailable"), true},
            {QStringLiteral("channelVolumes"),
             channelRows({QStringLiteral("FL"), QStringLiteral("FR")}, 50)},
            {QStringLiteral("channelVolumeAvailable"), true},
            {QStringLiteral("virtualDevice"), false},
            {QStringLiteral("stateText"), QStringLiteral("Default")},
        },
        QVariantMap{
            {QStringLiteral("serial"), qulonglong(12)},
            {QStringLiteral("kindText"), QStringLiteral("Output device")},
            {QStringLiteral("displayName"),
             QStringLiteral("Surround Headphones")},
            {QStringLiteral("volumePercent"), 25},
            {QStringLiteral("volumeKnown"), true},
            {QStringLiteral("muted"), false},
            {QStringLiteral("muteKnown"), true},
            {QStringLiteral("isDefault"), false},
            {QStringLiteral("setDefaultAvailable"), true},
            {QStringLiteral("volumeAvailable"), true},
            {QStringLiteral("muteAvailable"), true},
            {QStringLiteral("channelVolumes"),
             channelRows({QStringLiteral("FL"), QStringLiteral("FR"),
                          QStringLiteral("FC"), QStringLiteral("LFE"),
                          QStringLiteral("SL"), QStringLiteral("SR")},
                         25)},
            {QStringLiteral("channelVolumeAvailable"), true},
            {QStringLiteral("virtualDevice"), false},
            {QStringLiteral("stateText"), QStringLiteral("Volume 25%")},
        },
    };
}

inline QVariantList makeInputDevices()
{
    return {
        QVariantMap{
            {QStringLiteral("serial"), qulonglong(20)},
            {QStringLiteral("kindText"), QStringLiteral("Input device")},
            {QStringLiteral("displayName"),
             QStringLiteral("Desk Microphone")},
            {QStringLiteral("volumePercent"), 75},
            {QStringLiteral("volumeKnown"), true},
            {QStringLiteral("muted"), false},
            {QStringLiteral("muteKnown"), true},
            {QStringLiteral("isDefault"), true},
            {QStringLiteral("setDefaultAvailable"), true},
            {QStringLiteral("volumeAvailable"), true},
            {QStringLiteral("muteAvailable"), true},
            {QStringLiteral("channelVolumes"),
             channelRows({QStringLiteral("FL"), QStringLiteral("FR")}, 75)},
            {QStringLiteral("channelVolumeAvailable"), true},
            {QStringLiteral("virtualDevice"), false},
            {QStringLiteral("stateText"), QStringLiteral("Default")},
        },
    };
}

inline QVariantList makeStreams()
{
    return {
        QVariantMap{
            {QStringLiteral("serial"), qulonglong(30)},
            {QStringLiteral("directionText"), QStringLiteral("Playback")},
            {QStringLiteral("applicationName"), QStringLiteral("Player")},
            {QStringLiteral("mediaName"), QStringLiteral("Music")},
            {QStringLiteral("targetName"), QStringLiteral("Desk Speakers")},
            {QStringLiteral("volumePercent"), 75},
            {QStringLiteral("volumeKnown"), true},
            {QStringLiteral("muted"), false},
            {QStringLiteral("muteKnown"), true},
            {QStringLiteral("volumeAvailable"), true},
            {QStringLiteral("muteAvailable"), true},
        },
        QVariantMap{
            {QStringLiteral("serial"), qulonglong(40)},
            {QStringLiteral("directionText"), QStringLiteral("Recording")},
            {QStringLiteral("applicationName"), QStringLiteral("Talk")},
            {QStringLiteral("mediaName"), QStringLiteral("Call")},
            {QStringLiteral("targetName"),
             QStringLiteral("Desk Microphone")},
            {QStringLiteral("volumePercent"), 40},
            {QStringLiteral("volumeKnown"), true},
            {QStringLiteral("muted"), true},
            {QStringLiteral("muteKnown"), true},
            {QStringLiteral("volumeAvailable"), true},
            {QStringLiteral("muteAvailable"), true},
        },
    };
}

inline QVariantList makeVirtualDevices()
{
    return {
        QVariantMap{
            {QStringLiteral("serial"), qulonglong(14)},
            {QStringLiteral("kindText"),
             QStringLiteral("Virtual output device")},
            {QStringLiteral("displayName"), QStringLiteral("Game Bus")},
            {QStringLiteral("channelCount"), 2},
            {QStringLiteral("channelMap"),
             QStringLiteral("FL · FR")},
            {QStringLiteral("removeAvailable"), true},
        },
    };
}

inline QVariantMap consoleLevel(const double peakDb, const double rmsDb,
                                const bool known) {
  return QVariantMap{{QStringLiteral("peakDb"), peakDb},
                     {QStringLiteral("rmsDb"), rmsDb},
                     {QStringLiteral("known"), known}};
}

inline QVariantList consoleSends(const QList<int> &enabledBuses) {
  QVariantList sends;
  for (int index = 0; index < 3; ++index) {
    sends.append(QVariantMap{
        {QStringLiteral("busIndex"), index},
        {QStringLiteral("enabled"), enabledBuses.contains(index)},
        {QStringLiteral("gainDb"), 0.0},
    });
  }
  return sends;
}

inline QVariantMap consoleStripRow(const QString &id, const QString &label,
                                   const bool isVirtual, const bool bound,
                                   const QList<int> &enabledBuses) {
  return QVariantMap{
      {QStringLiteral("id"), id},
      {QStringLiteral("label"), label},
      {QStringLiteral("virtual"), isVirtual},
      {QStringLiteral("gainDb"), 0.0},
      {QStringLiteral("faderPosition"), 0.75},
      {QStringLiteral("muted"), false},
      {QStringLiteral("soloed"), false},
      {QStringLiteral("mono"), false},
      {QStringLiteral("pan"), 0.0},
      {QStringLiteral("bound"), bound},
      {QStringLiteral("sourceSerial"), bound ? qulonglong(20) : qulonglong(0)},
      {QStringLiteral("pinned"), false},
      {QStringLiteral("pinnedSource"), QString()},
      {QStringLiteral("sends"), consoleSends(enabledBuses)},
      {QStringLiteral("level"), consoleLevel(-96.0, -96.0, false)},
  };
}

inline QVariantMap consoleBusRow(const QString &id, const int index,
                                 const QString &label, const bool isVirtual,
                                 const bool bound) {
  return QVariantMap{
      {QStringLiteral("id"), id},
      {QStringLiteral("index"), index},
      {QStringLiteral("label"), label},
      {QStringLiteral("virtual"), isVirtual},
      {QStringLiteral("gainDb"), 0.0},
      {QStringLiteral("faderPosition"), 0.75},
      {QStringLiteral("muted"), false},
      {QStringLiteral("mono"), false},
      {QStringLiteral("bound"), bound},
      {QStringLiteral("targetSerial"), bound ? qulonglong(10) : qulonglong(0)},
      {QStringLiteral("pinned"), false},
      {QStringLiteral("pinnedTarget"), QString()},
      {QStringLiteral("level"), consoleLevel(-96.0, -96.0, false)},
  };
}

inline QVariantList makeConsoleStrips() {
  return {
      consoleStripRow(QStringLiteral("strip.hw.1"),
                      QStringLiteral("Desk Microphone"), false, true, {0}),
      consoleStripRow(QStringLiteral("strip.virtual.1"),
                      QStringLiteral("Game"), true, false, {0, 1}),
  };
}

inline QVariantList makeConsoleBuses() {
  return {
      consoleBusRow(QStringLiteral("bus.a1"), 0, QStringLiteral("A1"), false,
                    true),
      consoleBusRow(QStringLiteral("bus.a2"), 1, QStringLiteral("A2"), false,
                    false),
      consoleBusRow(QStringLiteral("bus.b1"), 2, QStringLiteral("B1"), true,
                    false),
  };
}

// One metered strip and one metered bus, so a row that asserts on the meter
// channel has something to read that the strip and bus rows themselves do not
// carry (ADR-0174).
inline QVariantMap makeConsoleLevels() {
  return QVariantMap{
      {QStringLiteral("strip.hw.1"), consoleLevel(-6.0, -14.0, true)},
      {QStringLiteral("bus.a1"), consoleLevel(-18.0, -24.0, true)},
  };
}

} // namespace QindaQt::Apps::SettingsAudio::TestSupport
