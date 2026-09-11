// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/audio_client/audio_client.h>

#include <QtCore/QObject>
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
// capability bits, per-target can-set flags, and the single-operation fence).
// An enabled control can therefore never be locally refused, and a disabled
// one is never dispatched. Do not widen one side without the other.
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

Q_SIGNALS:
  void viewChanged();
  void actionRejected(const QString &reason);

private:
  // Intent kinds the route can express; MoveStream is deliberately not part
  // of this slice's surface.
  enum class Intent { SetDefault, DeviceVolume, DeviceMute, StreamVolume,
                       StreamMute, DeviceChannelVolume, CreateVirtual,
                       RemoveVirtual };

  struct PendingIntent {
    quint64 requestId = 0;
    quint64 serial = 0;
    Intent intent = Intent::SetDefault;
  };

  void handleOperationCompleted(quint64 requestId,
                                const Audio::OperationResult &result);
  void beginIntentMessage(const Intent intent);
  void rejectAction(const QString &reason);
  [[nodiscard]] QString actionFailureText(const QString &reason) const;
  [[nodiscard]] bool dispatchDeviceIntent(quint64 serial, const Intent intent,
                                          double level, bool muted);
  [[nodiscard]] bool dispatchStreamIntent(quint64 serial, const Intent intent,
                                          double level, bool muted);
  void trackPending(quint64 requestId, quint64 serial, const Intent intent);

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
  std::optional<PendingIntent> m_pending;
};

} // namespace QindaQt::Apps::SettingsAudio
