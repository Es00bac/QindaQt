// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_client/audio_client.h>

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>

#include <optional>

namespace QindaQt::Apps::SettingsAudio {

// QML-safe, read-only projection and intent facade for one public Audio1
// client. The caller owns the client and must keep it alive on this object's
// Qt thread. This model never starts platform objects, touches WirePlumber or
// PipeWire, or manufactures graph truth from operation replies.
//
// AGENT-CONTRACT: Displayed action availability and dispatch admission are one
// predicate derived from the same public snapshot facts the AudioClient's
// dispatch preflight consumes (exact retained snapshot, its availability,
// capability bits and per-target can-set flags). Transport serialization is
// separate from availability: graph volume keeps one latest target per serial
// until a newer snapshot confirms the preceding write, and console gestures
// coalesce until the client is free. Other same-target intents remain fenced
// while pending; a disabled control is never dispatched.
class AudioSettingsModel final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading READ loading NOTIFY viewChanged)
  Q_PROPERTY(bool ready READ ready NOTIFY viewChanged)
  Q_PROPERTY(bool degraded READ degraded NOTIFY viewChanged)
  Q_PROPERTY(bool unavailable READ unavailable NOTIFY viewChanged)
  Q_PROPERTY(bool stale READ stale NOTIFY viewChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY viewChanged)
  Q_PROPERTY(bool reloadAvailable READ reloadAvailable NOTIFY viewChanged)
  Q_PROPERTY(QString statusText READ statusText NOTIFY viewChanged)
  Q_PROPERTY(QString errorText READ errorText NOTIFY viewChanged)
  Q_PROPERTY(QString operationStatusText READ operationStatusText NOTIFY
                 viewChanged)
  Q_PROPERTY(QString serviceOwner READ serviceOwner NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceEpoch READ serviceEpoch NOTIFY viewChanged)
  Q_PROPERTY(qulonglong serviceRevision READ serviceRevision NOTIFY viewChanged)
  Q_PROPERTY(QString defaultOutputName READ defaultOutputName NOTIFY viewChanged)
  Q_PROPERTY(QString defaultInputName READ defaultInputName NOTIFY viewChanged)
  Q_PROPERTY(QVariantList outputDevices READ outputDevices NOTIFY viewChanged)
  Q_PROPERTY(QVariantList inputDevices READ inputDevices NOTIFY viewChanged)
  Q_PROPERTY(QVariantList streams READ streams NOTIFY viewChanged)
  Q_PROPERTY(QVariantList virtualDevices READ virtualDevices NOTIFY viewChanged)
  Q_PROPERTY(bool canSetChannelVolumes READ canSetChannelVolumes NOTIFY
                 viewChanged)
  Q_PROPERTY(bool canManageVirtualDevices READ canManageVirtualDevices NOTIFY
                 viewChanged)
  // The mixing console (ADR-0173). `consoleAvailable` is false on a service
  // that publishes no console, and every console control disables with it.
  Q_PROPERTY(bool consoleAvailable READ consoleAvailable NOTIFY viewChanged)
  Q_PROPERTY(bool consoleSoloActive READ consoleSoloActive NOTIFY viewChanged)
  Q_PROPERTY(QVariantList consoleStrips READ consoleStrips NOTIFY viewChanged)
  Q_PROPERTY(QVariantList consoleBuses READ consoleBuses NOTIFY viewChanged)
  // Meters, on their own notification (ADR-0174). Deliberately NOT part of
  // viewChanged: levels arrive many times a second, and hanging them off the
  // view signal would rebuild every strip and bus delegate at meter rate.
  Q_PROPERTY(QVariantMap consoleLevels READ consoleLevels NOTIFY consoleLevelsChanged)
  Q_PROPERTY(QStringList consolePresets READ consolePresets NOTIFY viewChanged)
  Q_PROPERTY(QStringList consoleMacros READ consoleMacros NOTIFY viewChanged)
  Q_PROPERTY(QVariantMap consoleRecording READ consoleRecording NOTIFY viewChanged)
  Q_PROPERTY(QVariantList consoleVban READ consoleVban NOTIFY viewChanged)

public:
  explicit AudioSettingsModel(Audio::AudioClient &client,
                              QObject *parent = nullptr);

  [[nodiscard]] bool loading() const noexcept;
  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] bool degraded() const noexcept;
  [[nodiscard]] bool unavailable() const noexcept;
  [[nodiscard]] bool stale() const;
  [[nodiscard]] bool busy() const noexcept;
  [[nodiscard]] bool reloadAvailable() const noexcept;
  [[nodiscard]] bool consoleAvailable() const;
  [[nodiscard]] bool consoleSoloActive() const;
  [[nodiscard]] QVariantList consoleStrips() const;
  [[nodiscard]] QVariantList consoleBuses() const;
  // Console id to {peakDb, rmsDb, known}. Built from the snapshot the client
  // already holds, so the first read is correct before any reading arrives.
  [[nodiscard]] QVariantMap consoleLevels() const;
  [[nodiscard]] QStringList consolePresets() const;
  [[nodiscard]] QStringList consoleMacros() const;
  [[nodiscard]] QVariantMap consoleRecording() const;
  [[nodiscard]] QVariantList consoleVban() const;

  [[nodiscard]] QString statusText() const;
  [[nodiscard]] QString errorText() const;
  [[nodiscard]] const QString &operationStatusText() const noexcept {
    return m_operationStatusText;
  }
  [[nodiscard]] QString serviceOwner() const;
  [[nodiscard]] qulonglong serviceEpoch() const;
  [[nodiscard]] qulonglong serviceRevision() const;
  [[nodiscard]] QString defaultOutputName() const;
  [[nodiscard]] QString defaultInputName() const;
  [[nodiscard]] QVariantList outputDevices() const;
  [[nodiscard]] QVariantList inputDevices() const;
  [[nodiscard]] QVariantList streams() const;
  [[nodiscard]] QVariantList virtualDevices() const;
  [[nodiscard]] bool canSetChannelVolumes() const;
  [[nodiscard]] bool canManageVirtualDevices() const;

  // Serials are the snapshot-unique Audio1 object identity; the model
  // re-resolves them against the current snapshot before dispatch, so a
  // vanished or replaced-lineage target is refused locally instead of being
  // sent as a stale handle. A returned false means the request was refused
  // locally with feedback; it was never dispatched. Nothing here retries an
  // uncertain mutation.
  Q_INVOKABLE bool reload();
  Q_INVOKABLE bool setDefaultDevice(quint64 serial);
  Q_INVOKABLE bool setDeviceVolume(quint64 serial, double level);
  Q_INVOKABLE bool setDeviceMuted(quint64 serial, bool muted);
  Q_INVOKABLE bool setStreamVolume(quint64 serial, double level);
  Q_INVOKABLE bool setStreamMuted(quint64 serial, bool muted);
  // Per-channel volume is expressed as one channel of the device's retained
  // layout; the full-layout vector is rebuilt from the same snapshot the
  // client's preflight validates against.
  Q_INVOKABLE bool setDeviceChannelVolume(quint64 serial, int channelIndex,
                                          double level);
  // kindToken is the closed route vocabulary "output" or "input". The display
  // name is route-generated (never free user text) and channels is one of
  // 2, 4, 6, or 8.
  Q_INVOKABLE bool createVirtualDevice(QString kindToken, QString displayName,
                                       int channels);
  Q_INVOKABLE bool removeVirtualDevice(quint64 serial);

  // Console intents. Faders are driven by POSITION (0..1) from the UI and
  // converted through the one gain law (ADR-0171), so a slider and the dB
  // legend beside it can never disagree.
  Q_INVOKABLE bool setStripFader(QString stripId, double position);
  // Device pins (ADR-0178). `serial` names a device from inputDevices (for a
  // strip) or outputDevices (for a bus); 0 returns the element to automatic.
  // The strip's whole rack (ADR-0179), as the map consoleStrips publishes
  // under "processing" with any controls changed; refused whole when a value
  // is out of range.
  Q_INVOKABLE bool setStripProcessing(QString stripId, QVariantMap processing);
  // A bus's rack (ADR-0180): "equalizer" as for a strip, and "mode" as one of
  // normal, swap, left, right.
  Q_INVOKABLE bool setBusProcessing(QString busId, QVariantMap processing);
  // Presets (ADR-0182): whole console documents under a name.
  Q_INVOKABLE bool savePreset(QString name);
  Q_INVOKABLE bool loadPreset(QString name);
  Q_INVOKABLE bool deletePreset(QString name);
  // Runs a macro button (ADR-0183).
  Q_INVOKABLE bool runMacro(QString name);
  // The recorder (ADR-0184): one bus at a time, "flac" or "wav".
  Q_INVOKABLE bool startRecording(QString busId, QString format);
  Q_INVOKABLE bool stopRecording();
  // VBAN (ADR-0185): switch a defined stream on or off.
  Q_INVOKABLE bool setVbanEnabled(QString name, bool enabled);
  Q_INVOKABLE bool setStripSource(QString stripId, quint64 serial);
  Q_INVOKABLE bool setBusTarget(QString busId, quint64 serial);
  Q_INVOKABLE bool setStripMuted(QString stripId, bool muted);
  Q_INVOKABLE bool setStripSoloed(QString stripId, bool soloed);
  Q_INVOKABLE bool setStripMono(QString stripId, bool mono);
  Q_INVOKABLE bool setStripPan(QString stripId, double pan);
  Q_INVOKABLE bool setStripSend(QString stripId, int busIndex, bool enabled,
                                double gainDb);
  Q_INVOKABLE bool setBusFader(QString busId, double position);
  Q_INVOKABLE bool setBusMuted(QString busId, bool muted);
  Q_INVOKABLE bool setBusMono(QString busId, bool mono);
  // Pure helpers so QML draws the same scale the service applies.
  [[nodiscard]] Q_INVOKABLE double faderPositionForGain(double gainDb) const;
  [[nodiscard]] Q_INVOKABLE double gainForFaderPosition(double position) const;
  [[nodiscard]] Q_INVOKABLE double unityFaderPosition() const;

Q_SIGNALS:
  void viewChanged();
  void consoleLevelsChanged();
  void actionRejected(const QString &reason);

private:
  // Intent kinds the route can express; MoveStream is deliberately not part
  // of this slice's surface.
  enum class Intent { SetDefault, DeviceVolume, DeviceMute, StreamVolume,
                       StreamMute, DeviceChannelVolume, CreateVirtual,
                       RemoveVirtual };

  // Console intents (ADR-0173). Kept separate from Intent because they carry a
  // console id rather than a graph serial. AudioClient still fences authority
  // changes and every returned id is tracked for failure/uncertainty feedback.
  enum class ConsoleIntent { StripGain, StripMute, StripSolo, StripMono,
                              StripPan, StripSend, BusGain, BusMute, BusMono };

  struct PendingIntent {
    quint64 requestId = 0;
    Intent intent = Intent::SetDefault;
  };
  // One displayed latest target plus at most one queued successor per serial.
  // Authoritative volumePercent never comes from this transient intent.
  struct VolumeIntent {
    double displayLevel = 0.0;
    std::optional<double> queuedLevel;
    QString owner;
    quint64 epoch = 0;
    quint64 requestRevision = 0;
    quint64 generation = 0;
    bool isStream = false;
    bool awaitingSnapshot = false;
  };

  void handleOperationCompleted(quint64 requestId,
                                const Audio::OperationResult &result);
  void beginIntentMessage(const Intent intent);
  [[nodiscard]] bool trackConsoleRequest(quint64 requestId);
  void rejectAction(const QString &reason);
  [[nodiscard]] QString actionFailureText(const QString &reason) const;
  // AGENT-GUARD (mirrors ADR-0191 in the shell audio applet): pending state
  // is keyed per target serial, never one shared optional. A shared
  // "one pending intent" field made every row's volumeAvailable/muteAvailable
  // (snapshotAdmitsOperation) go false the instant any single control
  // dispatched, disabling the whole page for the duration of one request --
  // the applet's identical bug, here at the model layer instead of the
  // Repeater layer. `serial == 0` (CreateVirtual has no target device yet)
  // is tracked under the reserved key 0; only one create can be in flight at
  // a time, which matches there being only one "create" control on the page.
  [[nodiscard]] bool serialPending(quint64 serial) const {
    return m_pendingBySerial.contains(serial);
  }
  void trackPending(quint64 requestId, quint64 serial, const Intent intent);
  [[nodiscard]] bool requestVolume(quint64 serial, bool isStream, double level);
  void completeVolumeRequest(quint64 serial, Intent intent,
                             const Audio::OperationResult &result);
  void reconcileVolumeIntents();
  void expireVolumeIntent(quint64 serial, quint64 generation);
  [[nodiscard]] std::optional<double> displayVolumeLevel(quint64 serial,
                                                          bool isStream) const;
  [[nodiscard]] bool dispatchDeviceIntent(quint64 serial, const Intent intent,
                                          double level, bool muted);
  // One admission-and-dispatch path for every console control, so an enabled
  // control can never be locally refused and a disabled one never dispatched.
  [[nodiscard]] bool dispatchPin(bool strip, QString consoleId, quint64 serial);
  [[nodiscard]] bool dispatchPreset(Audio::OperationKind kind, QString name);
  [[nodiscard]] bool dispatchConsoleIntent(ConsoleIntent intent, QString consoleId,
                                            quint32 busIndex, double value,
                                            bool flag);
  [[nodiscard]] bool dispatchStreamIntent(quint64 serial, const Intent intent,
                                          double level, bool muted);

  // AGENT-GUARD: One shared admission predicate backs both displayed row
  // availability (audio_settings_projection.cpp) and dispatch refusal
  // (audio_settings_model.cpp). Never duplicate or widen it locally; the
  // availability/admission equality contract depends on the single
  // definition.
  [[nodiscard]] static bool snapshotAdmitsOperation(
      const Audio::AudioClient &client, Audio::Capability capability);
  [[nodiscard]] QVariantMap projectDeviceRow(const Audio::Device &device,
                                             bool isDefault) const;
  [[nodiscard]] QVariantMap projectStreamRow(
      const Audio::Stream &stream, const Audio::Device *target) const;

  Audio::AudioClient &m_client;
  QString m_localError;
  QString m_operationStatusText;
  QHash<quint64, PendingIntent> m_pendingBySerial;
  QHash<quint64, quint64> m_serialByRequestId;
  QHash<quint64, VolumeIntent> m_volumeBySerial;
  quint64 m_nextVolumeGeneration = 1;
  QSet<quint64> m_consoleRequestIds;
};

} // namespace QindaQt::Apps::SettingsAudio
