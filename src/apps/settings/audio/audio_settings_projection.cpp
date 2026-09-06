// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <qindaqt/services/audio_protocol/audio_types.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QVariantMap>

#include <cmath>

namespace QindaQt::Apps::SettingsAudio {
namespace {

using QindaQt::Audio::Capability;
using QindaQt::Audio::Device;
using QindaQt::Audio::DeviceKind;
using QindaQt::Audio::Snapshot;
using QindaQt::Audio::Stream;
using QindaQt::Audio::StreamDirection;

QString translateAudio(const char *text) {
  return QCoreApplication::translate("AudioSettings", text);
}

int percentFor(const double volume, const bool known) {
  if (!known) {
    return -1;
  }
  return static_cast<int>(std::round(qBound(0.0, volume, 1.0) * 100.0));
}

QString deviceDisplayName(const Device &device) {
  if (!device.description.trimmed().isEmpty()) {
    return device.description;
  }
  if (!device.name.trimmed().isEmpty()) {
    return device.name;
  }
  return translateAudio("Audio device");
}

const Device *findDeviceBySerial(const Snapshot &snapshot,
                                 const quint64 serial) {
  for (const Device &device : snapshot.outputs) {
    if (device.handle.serial == serial) {
      return &device;
    }
  }
  for (const Device &device : snapshot.inputs) {
    if (device.handle.serial == serial) {
      return &device;
    }
  }
  return nullptr;
}

} // namespace

// Row availability combines the snapshot-admission fence with the target's
// own advertised can-set flag, mirroring exactly what the client's dispatch
// preflight will accept for that target.
QVariantMap AudioSettingsModel::projectDeviceRow(const Device &device,
                                                 const bool isDefault) const {
  QStringList state;
  if (isDefault) {
    state.append(translateAudio("Default"));
  }
  if (device.muteKnown && device.muted) {
    state.append(translateAudio("Muted"));
  } else {
    state.append(device.volumeKnown
                     ? translateAudio("Volume %1%").arg(
                           percentFor(device.volume, device.volumeKnown))
                     : translateAudio("Volume unknown"));
  }
  return QVariantMap{
      {QStringLiteral("serial"), device.handle.serial},
      {QStringLiteral("kindText"),
       device.kind == DeviceKind::Output ? translateAudio("Output device")
                                          : translateAudio("Input device")},
      {QStringLiteral("displayName"), deviceDisplayName(device)},
      {QStringLiteral("volumePercent"),
       percentFor(device.volume, device.volumeKnown)},
      {QStringLiteral("volumeKnown"), device.volumeKnown},
      {QStringLiteral("muted"), device.muteKnown && device.muted},
      {QStringLiteral("muteKnown"), device.muteKnown},
      {QStringLiteral("isDefault"), isDefault},
      {QStringLiteral("setDefaultAvailable"),
       snapshotAdmitsOperation(m_client, Capability::SetDefault)},
      {QStringLiteral("volumeAvailable"),
       device.canSetVolume
           && snapshotAdmitsOperation(m_client, Capability::SetVolume)},
      {QStringLiteral("muteAvailable"),
       device.canSetMute
           && snapshotAdmitsOperation(m_client, Capability::SetMute)},
      {QStringLiteral("stateText"), state.join(QStringLiteral(" · "))},
  };
}

QVariantMap AudioSettingsModel::projectStreamRow(
    const Stream &stream, const Device *target) const {
  return QVariantMap{
      {QStringLiteral("serial"), stream.handle.serial},
      {QStringLiteral("directionText"),
       stream.direction == StreamDirection::Playback ? translateAudio("Playback")
                                                     : translateAudio("Recording")},
      {QStringLiteral("applicationName"),
       stream.applicationName.trimmed().isEmpty()
           ? translateAudio("Application")
           : stream.applicationName},
      {QStringLiteral("mediaName"), stream.mediaName},
      {QStringLiteral("targetName"),
       target != nullptr ? deviceDisplayName(*target)
                         : translateAudio("Unknown device")},
      {QStringLiteral("volumePercent"),
       percentFor(stream.volume, stream.volumeKnown)},
      {QStringLiteral("volumeKnown"), stream.volumeKnown},
      {QStringLiteral("muted"), stream.muteKnown && stream.muted},
      {QStringLiteral("muteKnown"), stream.muteKnown},
      {QStringLiteral("volumeAvailable"),
       stream.canSetVolume
           && snapshotAdmitsOperation(m_client, Capability::SetVolume)},
      {QStringLiteral("muteAvailable"),
       stream.canSetMute
           && snapshotAdmitsOperation(m_client, Capability::SetMute)},
  };
}

QString AudioSettingsModel::statusText() const {
  if (loading()) {
    return translateAudio("Connecting to the audio service…");
  }
  if (ready()) {
    return translateAudio("Choose your speakers and microphone, and adjust their volume.");
  }
  if (stale()) {
    return translateAudio("Audio information is stale while the service recovers.");
  }
  if (degraded()) {
    return translateAudio("Audio information is limited right now.");
  }
  return translateAudio("The audio service is unavailable.");
}

QString AudioSettingsModel::errorText() const { return m_localError; }

QString AudioSettingsModel::defaultOutputName() const {
  if (!m_client.hasSnapshot()) {
    return {};
  }
  const Snapshot snapshot = m_client.snapshot();
  const Device *device =
      snapshot.defaultOutput.isValid()
          ? findDeviceBySerial(snapshot, snapshot.defaultOutput.serial)
          : nullptr;
  return device == nullptr ? translateAudio("No default output device")
                           : deviceDisplayName(*device);
}

QString AudioSettingsModel::defaultInputName() const {
  if (!m_client.hasSnapshot()) {
    return {};
  }
  const Snapshot snapshot = m_client.snapshot();
  const Device *device =
      snapshot.defaultInput.isValid()
          ? findDeviceBySerial(snapshot, snapshot.defaultInput.serial)
          : nullptr;
  return device == nullptr ? translateAudio("No default input device")
                           : deviceDisplayName(*device);
}

QVariantList AudioSettingsModel::outputDevices() const {
  QVariantList rows;
  if (!m_client.hasSnapshot()) {
    return rows;
  }
  const Snapshot snapshot = m_client.snapshot();
  rows.reserve(snapshot.outputs.size());
  for (const Device &device : snapshot.outputs) {
    const bool isDefault = snapshot.defaultOutput.isValid()
                           && device.handle == snapshot.defaultOutput;
    rows.append(projectDeviceRow(device, isDefault));
  }
  return rows;
}

QVariantList AudioSettingsModel::inputDevices() const {
  QVariantList rows;
  if (!m_client.hasSnapshot()) {
    return rows;
  }
  const Snapshot snapshot = m_client.snapshot();
  rows.reserve(snapshot.inputs.size());
  for (const Device &device : snapshot.inputs) {
    const bool isDefault = snapshot.defaultInput.isValid()
                           && device.handle == snapshot.defaultInput;
    rows.append(projectDeviceRow(device, isDefault));
  }
  return rows;
}

QVariantList AudioSettingsModel::streams() const {
  QVariantList rows;
  if (!m_client.hasSnapshot()) {
    return rows;
  }
  const Snapshot snapshot = m_client.snapshot();
  rows.reserve(snapshot.streams.size());
  for (const Stream &stream : snapshot.streams) {
    const Device *target =
        stream.targetKnown && stream.target.isValid()
            ? findDeviceBySerial(snapshot, stream.target.serial)
            : nullptr;
    rows.append(projectStreamRow(stream, target));
  }
  return rows;
}

} // namespace QindaQt::Apps::SettingsAudio
