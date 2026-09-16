// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include "stub_audio_fixture.h"

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
  Q_PROPERTY(bool consoleAvailable MEMBER consoleAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool consoleSoloActive MEMBER consoleSoloActive NOTIFY viewChanged)
  Q_PROPERTY(QVariantList consoleStrips MEMBER consoleStrips NOTIFY viewChanged)
  Q_PROPERTY(QVariantList consoleBuses MEMBER consoleBuses NOTIFY viewChanged)
  Q_PROPERTY(QVariantMap consoleLevels MEMBER consoleLevels NOTIFY
                 consoleLevelsChanged)
  Q_PROPERTY(QStringList consolePresets MEMBER consolePresets NOTIFY viewChanged)

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
  bool consoleAvailable = true;
  bool consoleSoloActive = false;
  QVariantList consoleStrips;
  QVariantList consoleBuses;
  QVariantMap consoleLevels;
  QStringList consolePresets = {QStringLiteral("Stream night"), QStringLiteral("Podcast")};
  QString lastPresetName;
  QString lastConsoleId;
  double lastFaderPosition = -1.0;
  bool lastConsoleFlag = false;
  int lastBusIndex = -1;
  quint64 lastPinSerial = 0;
  QVariantMap lastProcessing;
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

  explicit StubAudioSettingsModel(QObject *parent = nullptr)
      : QObject(parent) {
    outputDevices = makeOutputDevices();
    inputDevices = makeInputDevices();
    streams = makeStreams();
    virtualDevices = makeVirtualDevices();
    consoleStrips = makeConsoleStrips();
    consoleBuses = makeConsoleBuses();
    consoleLevels = makeConsoleLevels();
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

  // The console intent surface (ADR-0173) and the fader conversions QML draws
  // with. Every entry mirrors the real model exactly; the surface comparison
  // row is what keeps them from drifting apart.
  Q_INVOKABLE bool setStripProcessing(QString stripId, QVariantMap processing) {
    lastConsoleId = stripId;
    lastProcessing = processing;
    return true;
  }
  Q_INVOKABLE bool savePreset(QString name) {
    lastPresetName = name;
    return true;
  }
  Q_INVOKABLE bool loadPreset(QString name) {
    lastPresetName = name;
    return true;
  }
  Q_INVOKABLE bool deletePreset(QString name) {
    lastPresetName = name;
    return true;
  }
  Q_INVOKABLE bool setBusProcessing(QString busId, QVariantMap processing) {
    lastConsoleId = busId;
    lastProcessing = processing;
    return true;
  }
  Q_INVOKABLE bool setStripSource(QString stripId, quint64 serial) {
    lastConsoleId = stripId;
    lastPinSerial = serial;
    return true;
  }
  Q_INVOKABLE bool setBusTarget(QString busId, quint64 serial) {
    lastConsoleId = busId;
    lastPinSerial = serial;
    return true;
  }
  Q_INVOKABLE bool setStripFader(QString stripId, double position) {
    lastConsoleId = stripId;
    lastFaderPosition = position;
    return true;
  }
  Q_INVOKABLE bool setStripMuted(QString stripId, bool muted) {
    lastConsoleId = stripId;
    lastConsoleFlag = muted;
    return true;
  }
  Q_INVOKABLE bool setStripSoloed(QString stripId, bool soloed) {
    lastConsoleId = stripId;
    lastConsoleFlag = soloed;
    return true;
  }
  Q_INVOKABLE bool setStripMono(QString stripId, bool mono) {
    lastConsoleId = stripId;
    lastConsoleFlag = mono;
    return true;
  }
  Q_INVOKABLE bool setStripPan(QString stripId, double pan) {
    lastConsoleId = stripId;
    lastFaderPosition = pan;
    return true;
  }
  Q_INVOKABLE bool setStripSend(QString stripId, int busIndex, bool enabled,
                                double gainDb) {
    lastConsoleId = stripId;
    lastBusIndex = busIndex;
    lastConsoleFlag = enabled;
    lastFaderPosition = gainDb;
    return true;
  }
  Q_INVOKABLE bool setBusFader(QString busId, double position) {
    lastConsoleId = busId;
    lastFaderPosition = position;
    return true;
  }
  Q_INVOKABLE bool setBusMuted(QString busId, bool muted) {
    lastConsoleId = busId;
    lastConsoleFlag = muted;
    return true;
  }
  Q_INVOKABLE bool setBusMono(QString busId, bool mono) {
    lastConsoleId = busId;
    lastConsoleFlag = mono;
    return true;
  }
  Q_INVOKABLE double faderPositionForGain(double gainDb) {
    return gainDb >= 0.0 ? 1.0 : 0.75;
  }
  Q_INVOKABLE double gainForFaderPosition(double position) {
    return position >= 1.0 ? 12.0 : 0.0;
  }
  Q_INVOKABLE double unityFaderPosition() { return 0.75; }

Q_SIGNALS:
  void viewChanged();
  void consoleLevelsChanged();
};

} // namespace QindaQt::Apps::SettingsAudio::TestSupport
