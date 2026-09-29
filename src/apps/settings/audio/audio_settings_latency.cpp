// SPDX-License-Identifier: LGPL-3.0-or-later

// Per-device latency offset intent and projection (ADR-0288). Milliseconds at
// this boundary, nanoseconds on the wire; one admission rule with Audio1
// (latencyOffsetAdmitted), so an enabled control is never refused locally.
#include <qindaqt/apps/settings_audio/audio_settings_model.h>

#include <qindaqt/services/audio_protocol/audio_validation.h>

#include <QtCore/QCoreApplication>
#include <QtCore/QTimer>

#include <cmath>

namespace QindaQt::Apps::SettingsAudio {
namespace {

using Audio::Capability;
using Audio::Device;
using Audio::OperationResult;
using Audio::OperationStatus;
using Audio::Snapshot;

constexpr qint64 kNsPerMs = 1'000'000;
// How long a dispatched target stays on screen without a confirming snapshot
// before the row returns to what the device reports.
constexpr int kLatencyReadbackMs = 3'000;

QString translateAudio(const char *text) {
  return QCoreApplication::translate("AudioSettings", text);
}

int roundedMs(const qint64 ns) {
  return static_cast<int>(std::llround(static_cast<double>(ns) / kNsPerMs));
}

const Device *findAnyDevice(const Snapshot &snapshot, const quint64 serial) {
  for (const QList<Device> *devices : {&snapshot.outputs, &snapshot.inputs}) {
    for (const Device &device : *devices) {
      if (device.handle.serial == serial) {
        return &device;
      }
    }
  }
  return nullptr;
}

} // namespace

QVariantMap AudioSettingsModel::projectLatency(const Device &device) const {
  const bool available =
      device.canSetLatencyOffset
      && snapshotAdmitsOperation(m_client, Capability::SetLatencyOffset);
  const auto intent = m_latencyBySerial.constFind(device.handle.serial);
  const qint64 displayNs = intent != m_latencyBySerial.constEnd()
                               ? intent->displayNs
                               : device.latencyOffsetNs;
  // AGENT-CONTRACT: an unknown offset is absent (latencyKnown false, no text
  // number), never a 0 ms reading; the settable bounds are whole
  // milliseconds inside the device's nanosecond range.
  return QVariantMap{
      {QStringLiteral("latencyKnown"), device.latencyOffsetKnown},
      {QStringLiteral("latencyAvailable"), available},
      {QStringLiteral("latencyMs"), roundedMs(device.latencyOffsetNs)},
      {QStringLiteral("latencyDisplayMs"), roundedMs(displayNs)},
      {QStringLiteral("latencyMinMs"),
       static_cast<int>(std::ceil(static_cast<double>(device.latencyOffsetMinNs) / kNsPerMs))},
      {QStringLiteral("latencyMaxMs"),
       static_cast<int>(std::floor(static_cast<double>(device.latencyOffsetMaxNs) / kNsPerMs))},
      {QStringLiteral("latencyText"),
       device.latencyOffsetKnown
           ? translateAudio("%1 ms").arg(roundedMs(device.latencyOffsetNs))
           : translateAudio("Latency offset unknown")},
      {QStringLiteral("latencyPending"), intent != m_latencyBySerial.constEnd()},
  };
}

bool AudioSettingsModel::setDeviceLatencyOffset(const quint64 serial,
                                                const int milliseconds) {
  const qint64 offsetNs = static_cast<qint64>(milliseconds) * kNsPerMs;
  const auto intent = m_latencyBySerial.find(serial);
  if (intent != m_latencyBySerial.end() && intent->inFlight) {
    // One latest successor per device, like volume: rapid steps replace the
    // queued value instead of being refused while the first is answered.
    const Device *device = m_client.hasSnapshot()
                               ? findAnyDevice(m_client.snapshot(), serial)
                               : nullptr;
    if (device == nullptr || !Audio::latencyOffsetAdmitted(*device, offsetNs)) {
      rejectAction(device == nullptr ? QStringLiteral("stale-handle")
                                     : QStringLiteral("invalid-latency-offset"));
      return false;
    }
    intent->queuedNs = offsetNs;
    intent->displayNs = offsetNs;
    Q_EMIT viewChanged();
    return true;
  }
  return dispatchLatency(serial, offsetNs);
}

bool AudioSettingsModel::dispatchLatency(const quint64 serial,
                                         const qint64 offsetNs) {
  if (!snapshotAdmitsOperation(m_client, Capability::SetLatencyOffset)) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  if (serialPending(serial)) {
    rejectAction(QStringLiteral("operation-in-flight"));
    return false;
  }
  const Device *device = findAnyDevice(m_client.snapshot(), serial);
  if (device == nullptr) {
    rejectAction(QStringLiteral("stale-handle"));
    return false;
  }
  if (!device->canSetLatencyOffset) {
    rejectAction(QStringLiteral("unsupported"));
    return false;
  }
  if (!Audio::latencyOffsetAdmitted(*device, offsetNs)) {
    rejectAction(QStringLiteral("invalid-latency-offset"));
    return false;
  }
  const quint64 requestId = m_client.setLatencyOffset(device->handle, offsetNs);
  if (requestId == 0) {
    m_latencyBySerial.remove(serial);
    rejectAction(QString());
    return false;
  }
  const quint64 generation = m_nextLatencyGeneration++;
  m_latencyBySerial.insert(serial, LatencyIntent{.displayNs = offsetNs,
                                                 .queuedNs = std::nullopt,
                                                 .generation = generation,
                                                 .inFlight = true});
  trackPending(requestId, serial, Intent::DeviceLatency);
  beginIntentMessage(Intent::DeviceLatency);
  QTimer::singleShot(kLatencyReadbackMs, this, [this, serial, generation] {
    const auto it = m_latencyBySerial.constFind(serial);
    if (it == m_latencyBySerial.constEnd() || it->generation != generation
        || it->inFlight) {
      return;
    }
    // The device never reported the value: show what it does report, and
    // say so rather than leaving an unconfirmed number on screen.
    m_latencyBySerial.remove(serial);
    m_operationStatusText.clear();
    m_localError = translateAudio(
        "The device has not reported the new latency offset. It shows the "
        "value the device reports now.");
    Q_EMIT viewChanged();
  });
  return true;
}

void AudioSettingsModel::completeLatencyRequest(const quint64 serial,
                                                const Intent intent,
                                                const OperationResult &result) {
  if (intent != Intent::DeviceLatency) {
    return;
  }
  const auto it = m_latencyBySerial.find(serial);
  if (it == m_latencyBySerial.end()) {
    return;
  }
  it->inFlight = false;
  if (result.status != OperationStatus::Succeeded) {
    // Refused or uncertain: the row falls back to the device's own report.
    m_latencyBySerial.erase(it);
    Q_EMIT viewChanged();
    return;
  }
  if (it->queuedNs.has_value()) {
    const qint64 next = *it->queuedNs;
    it->queuedNs.reset();
    if (!dispatchLatency(serial, next)) {
      m_latencyBySerial.remove(serial);
    }
    Q_EMIT viewChanged();
    return;
  }
  // A snapshot may already carry the value; AudioClient refetches before
  // it delivers the completion.
  reconcileLatencyIntents();
  Q_EMIT viewChanged();
}

void AudioSettingsModel::reconcileLatencyIntents() {
  if (m_latencyBySerial.isEmpty()) {
    return;
  }
  const bool hasSnapshot = m_client.hasSnapshot() && !m_client.owner().isEmpty();
  const Snapshot snapshot = hasSnapshot ? m_client.snapshot() : Snapshot{};
  for (auto it = m_latencyBySerial.begin(); it != m_latencyBySerial.end();) {
    const Device *device = hasSnapshot ? findAnyDevice(snapshot, it.key()) : nullptr;
    const bool gone = device == nullptr;
    const bool confirmed = device != nullptr && !it->inFlight
                           && !it->queuedNs.has_value() && device->latencyOffsetKnown
                           && device->latencyOffsetNs == it->displayNs;
    // AGENT-GUARD: an in-flight entry survives an unrelated snapshot so its
    // completion still finds it; only a vanished device or an owner/epoch
    // replacement (which removes the device) drops it early.
    it = gone || confirmed ? m_latencyBySerial.erase(it) : std::next(it);
  }
}

} // namespace QindaQt::Apps::SettingsAudio
