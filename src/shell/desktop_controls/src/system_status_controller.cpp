// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/system_status_controller.h"

#include "audio_applet_controller.h"
#include "bluetooth_applet_controller.h"
#include "power_applet_controller.h"

#include <QRegularExpression>
#include <QVariantMap>

namespace QindaQt::Shell::DesktopControls {
namespace {

QString batteryIconFor(bool hasBattery, const QString &batteryLabel)
{
  if (!hasBattery) {
    return QStringLiteral("battery-missing");
  }
  static const QRegularExpression percent(QStringLiteral("([0-9]+)%"));
  const QRegularExpressionMatch match = percent.match(batteryLabel);
  if (!match.hasMatch()) {
    return QStringLiteral("battery-missing");
  }
  const int level = match.captured(1).toInt();
  if (level >= 90) return QStringLiteral("battery-100");
  if (level >= 70) return QStringLiteral("battery-080");
  if (level >= 50) return QStringLiteral("battery-060");
  if (level >= 30) return QStringLiteral("battery-040");
  if (level >= 10) return QStringLiteral("battery-020");
  return QStringLiteral("battery-000");
}

QString volumeIconFor(bool muted, bool volumeKnown, double volume)
{
  if (muted) {
    return QStringLiteral("audio-volume-muted");
  }
  if (!volumeKnown) {
    return QStringLiteral("audio-volume-high");
  }
  if (volume <= 0.33) return QStringLiteral("audio-volume-low");
  if (volume <= 0.66) return QStringLiteral("audio-volume-medium");
  return QStringLiteral("audio-volume-high");
}

} // namespace

SystemStatusController::SystemStatusController(
    AudioApplet::AudioAppletController *audio,
    BluetoothApplet::BluetoothAppletController *bluetooth,
    PowerApplet::PowerAppletController *power, SystemStatusGrants grants,
    QObject *parent)
    : QObject(parent)
    , m_audio(grants.audioRead ? audio : nullptr)
    , m_bluetooth(grants.bluetoothRead ? bluetooth : nullptr)
    , m_power(grants.powerRead ? power : nullptr)
    , m_grants(grants)
{
  if (m_audio != nullptr) {
    connect(m_audio, &AudioApplet::AudioAppletController::stateReprojected, this,
            &SystemStatusController::reproject);
  }
  if (m_bluetooth != nullptr) {
    connect(m_bluetooth, &BluetoothApplet::BluetoothAppletController::stateChanged,
            this, &SystemStatusController::reproject);
  }
  if (m_power != nullptr) {
    connect(m_power, &PowerApplet::PowerAppletController::stateChanged, this,
            &SystemStatusController::reproject);
  }
  reproject();
}

QVariantList SystemStatusController::laneRows() const
{
  QVariantList rows;
  rows.reserve(m_model.lanes.size());
  for (const StatusLaneRow &lane : m_model.lanes) {
    rows.append(QVariantMap{
        {QStringLiteral("id"), lane.id},
        {QStringLiteral("label"), lane.label},
        {QStringLiteral("phase"), lane.phase},
        {QStringLiteral("summary"), lane.summary},
        {QStringLiteral("iconName"), lane.iconName},
        {QStringLiteral("accessibleName"), lane.accessibleName},
        {QStringLiteral("accessibleDescription"), lane.accessibleDescription},
        {QStringLiteral("available"), lane.available},
        {QStringLiteral("attention"), lane.attention},
    });
  }
  return rows;
}

QObject *SystemStatusController::audio() const noexcept
{
  return m_audio;
}

QObject *SystemStatusController::bluetooth() const noexcept
{
  return m_bluetooth;
}

QObject *SystemStatusController::power() const noexcept
{
  return m_power;
}

StatusLaneInput SystemStatusController::audioLane() const
{
  StatusLaneInput lane;
  lane.id = QStringLiteral("audio");
  lane.label = QStringLiteral("Sound");
  lane.granted = m_grants.audioRead;
  lane.present = m_audio != nullptr;
  if (!lane.present) {
    return lane;
  }
  lane.phase = m_audio->phaseText();
  lane.accessibleDescription = m_audio->phaseReasonText();
  bool haveDefault = false;
  for (const QVariant &value : m_audio->deviceRows()) {
    const auto row = value.value<AudioApplet::DeviceRow>();
    if (!row.isOutput() || !row.isDefault()) {
      continue;
    }
    haveDefault = true;
    lane.iconName = volumeIconFor(row.muteKnown() && row.muted(),
                                  row.volumeKnown(), row.volume());
    if (row.muteKnown() && row.muted()) {
      lane.summary = QStringLiteral("%1 muted").arg(row.label());
    } else if (row.volumeKnown()) {
      lane.summary = QStringLiteral("%1 at %2%")
                         .arg(row.label())
                         .arg(qRound(row.volume() * 100.0));
    } else {
      lane.summary = row.label();
    }
    break;
  }
  if (!haveDefault) {
    lane.iconName = QStringLiteral("audio-volume-muted");
    lane.summary = lane.phase == QLatin1StringView("ready")
        ? QStringLiteral("No output device")
        : QStringLiteral("Sound %1").arg(lane.phase);
  }
  lane.accessibleName = lane.summary;
  return lane;
}

StatusLaneInput SystemStatusController::bluetoothLane() const
{
  StatusLaneInput lane;
  lane.id = QStringLiteral("bluetooth");
  lane.label = QStringLiteral("Bluetooth");
  lane.granted = m_grants.bluetoothRead;
  lane.present = m_bluetooth != nullptr;
  if (!lane.present) {
    return lane;
  }
  lane.phase = m_bluetooth->phase();
  lane.summary = m_bluetooth->summaryLabel();
  lane.accessibleName = m_bluetooth->accessibleName();
  lane.accessibleDescription = m_bluetooth->accessibleDescription();
  bool connected = false;
  for (const QVariant &value : m_bluetooth->deviceRows()) {
    connected = connected || value.toMap().value(QStringLiteral("connected")).toBool();
  }
  bool powered = false;
  for (const QVariant &value : m_bluetooth->adapterRows()) {
    powered = powered || value.toMap().value(QStringLiteral("powered")).toBool();
  }
  lane.iconName = connected || powered
      ? QStringLiteral("network-bluetooth-activated")
      : QStringLiteral("network-bluetooth-inactive-symbolic");
  return lane;
}

StatusLaneInput SystemStatusController::powerLane() const
{
  StatusLaneInput lane;
  lane.id = QStringLiteral("power");
  lane.label = QStringLiteral("Power");
  lane.granted = m_grants.powerRead;
  lane.present = m_power != nullptr;
  if (!lane.present) {
    return lane;
  }
  lane.phase = m_power->phase();
  lane.summary = m_power->batteryLabel();
  lane.accessibleName = m_power->accessibleName();
  lane.accessibleDescription = m_power->accessibleDescription();
  lane.iconName = batteryIconFor(m_power->hasBattery(), lane.summary);
  lane.attention = lane.iconName == QLatin1StringView("battery-000")
      || lane.iconName == QLatin1StringView("battery-020");
  return lane;
}

void SystemStatusController::reproject()
{
  m_model = projectSystemStatus({audioLane(), bluetoothLane(), powerLane()});
  Q_EMIT stateChanged();
}

} // namespace QindaQt::Shell::DesktopControls
