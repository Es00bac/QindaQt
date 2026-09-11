// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsAudio::TestSupport {

// Offscreen page fixture mirroring the real AudioSettingsModel surface. The
// page test additionally compares property and invokable surfaces against the
// real model so the stub cannot drift.
class StubAudioSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading MEMBER loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready MEMBER ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded MEMBER degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable MEMBER unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale MEMBER stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy MEMBER busy NOTIFY viewChanged)
  Q_PROPERTY(bool reloadAvailable MEMBER reloadAvailable NOTIFY viewChanged)
  Q_PROPERTY(QString statusText MEMBER statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText MEMBER errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText MEMBER operationStatusText NOTIFY
                 viewChanged)
  Q_PROPERTY(QString serviceOwner MEMBER serviceOwner NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceEpoch MEMBER serviceEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceRevision MEMBER serviceRevision NOTIFY
                 viewChanged)
  Q_PROPERTY(QString defaultOutputName MEMBER defaultOutputName NOTIFY
                 viewChanged)
  Q_PROPERTY(QString defaultInputName MEMBER defaultInputName NOTIFY
                 viewChanged)
  Q_PROPERTY(QVariantList outputDevices MEMBER outputDevices NOTIFY viewChanged)
  Q_PROPERTY(QVariantList inputDevices MEMBER inputDevices NOTIFY viewChanged)
  Q_PROPERTY(QVariantList streams MEMBER streams NOTIFY viewChanged)
  Q_PROPERTY(QVariantList virtualDevices MEMBER virtualDevices NOTIFY
                 viewChanged)
  Q_PROPERTY(bool canSetChannelVolumes MEMBER canSetChannelVolumes NOTIFY
                 viewChanged)
  Q_PROPERTY(bool canManageVirtualDevices MEMBER canManageVirtualDevices NOTIFY
                 viewChanged)

public:
  bool loading = false;
  bool ready = true;
  bool degraded = false;
  bool unavailable = false;
  bool stale = false;
  bool busy = false;
  bool reloadAvailable = true;
  QString statusText = QStringLiteral("Authoritative audio state is shown.");
  QString errorText;
  QString operationStatusText;
  QString serviceOwner = QStringLiteral(":1.7");
  qulonglong serviceEpoch = 11;
  qulonglong serviceRevision = 2;
  QString defaultOutputName = QStringLiteral("Desk Speakers");
  QString defaultInputName = QStringLiteral("Desk Microphone");
  QVariantList outputDevices;
  QVariantList inputDevices;
  QVariantList streams;
  QVariantList virtualDevices;
  bool canSetChannelVolumes = true;
  bool canManageVirtualDevices = true;
  int reloadCount = 0;
  quint64 defaultSerial = 0;
  quint64 deviceVolumeSerial = 0;
  double deviceVolumeLevel = -1.0;
  quint64 deviceMuteSerial = 0;
  bool deviceMuted = false;
  quint64 streamVolumeSerial = 0;
  double streamVolumeLevel = -1.0;
  quint64 streamMuteSerial = 0;
  bool streamMuted = false;
  quint64 channelSerial = 0;
  int channelIndex = -1;
  double channelLevel = -1.0;
  QString createKindToken;
  QString createDisplayName;
  int createChannels = 0;
  int createCount = 0;
  quint64 removeVirtualSerial = 0;

  static QVariantList channelRows(const QStringList &positions,
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

  explicit StubAudioSettingsModel(QObject *parent = nullptr)
      : QObject(parent) {
    outputDevices = {
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
    inputDevices = {
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
    streams = {
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
    virtualDevices = {
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

  Q_INVOKABLE bool reload() {
    ++reloadCount;
    return true;
  }
  Q_INVOKABLE bool setDefaultDevice(quint64 serial) {
    defaultSerial = serial;
    return true;
  }
  Q_INVOKABLE bool setDeviceVolume(quint64 serial, double level) {
    deviceVolumeSerial = serial;
    deviceVolumeLevel = level;
    return true;
  }
  Q_INVOKABLE bool setDeviceMuted(quint64 serial, bool muted) {
    deviceMuteSerial = serial;
    deviceMuted = muted;
    return true;
  }
  Q_INVOKABLE bool setStreamVolume(quint64 serial, double level) {
    streamVolumeSerial = serial;
    streamVolumeLevel = level;
    return true;
  }
  Q_INVOKABLE bool setStreamMuted(quint64 serial, bool muted) {
    streamMuteSerial = serial;
    streamMuted = muted;
    return true;
  }
  Q_INVOKABLE bool setDeviceChannelVolume(quint64 serial, int index,
                                          double level) {
    channelSerial = serial;
    channelIndex = index;
    channelLevel = level;
    return true;
  }
  Q_INVOKABLE bool createVirtualDevice(QString kindToken, QString displayName,
                                       int channels) {
    ++createCount;
    createKindToken = kindToken;
    createDisplayName = displayName;
    createChannels = channels;
    return true;
  }
  Q_INVOKABLE bool removeVirtualDevice(quint64 serial) {
    removeVirtualSerial = serial;
    return true;
  }

Q_SIGNALS:
  void viewChanged();
};

} // namespace QindaQt::Apps::SettingsAudio::TestSupport
