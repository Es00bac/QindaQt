// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/shell/desktop_controls/system_status_presentation.h"

#include <QObject>
#include <QVariantList>

namespace QindaQt::Shell::AudioApplet {
class AudioAppletController;
}
namespace QindaQt::Shell::BluetoothApplet {
class BluetoothAppletController;
}
namespace QindaQt::Shell::PowerApplet {
class PowerAppletController;
}

namespace QindaQt::Shell::DesktopControls {

// This applet's own manifest grants. Read gates whether a lane is aggregated
// at all; control gates whether the popup offers that lane's quick controls.
// The borrowed facades additionally enforce their own manifest grants, so a
// mutation needs both applets' grants to reach a service.
struct SystemStatusGrants {
  bool audioRead = false;
  bool audioControl = false;
  bool bluetoothRead = false;
  bool bluetoothControl = false;
  bool powerRead = false;
  bool powerControl = false;

  friend bool operator==(const SystemStatusGrants &, const SystemStatusGrants &) =
      default;
};

// Aggregates the existing audio, Bluetooth, and power applet facades into one
// compact indicator. It owns no client, transport, or request state of its
// own: every quick control in the popup re-enters the borrowed facade.
//
// AGENT-CONTRACT: all borrowed facades may be null (lane absent) and must
// otherwise outlive this controller on the GUI thread. Shell composition owns
// their lifetimes; this object only observes and reprojects.
class SystemStatusController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList laneRows READ laneRows NOTIFY stateChanged)
  Q_PROPERTY(int availableLaneCount READ availableLaneCount NOTIFY stateChanged)
  Q_PROPERTY(int laneCount READ laneCount NOTIFY stateChanged)
  Q_PROPERTY(bool anyLaneAvailable READ anyLaneAvailable NOTIFY stateChanged)
  Q_PROPERTY(QString accessibleName READ accessibleName NOTIFY stateChanged)
  Q_PROPERTY(QString accessibleDescription READ accessibleDescription NOTIFY stateChanged)
  Q_PROPERTY(QObject *audio READ audio CONSTANT)
  Q_PROPERTY(QObject *bluetooth READ bluetooth CONSTANT)
  Q_PROPERTY(QObject *power READ power CONSTANT)
  Q_PROPERTY(bool audioControlGranted READ audioControlGranted CONSTANT)
  Q_PROPERTY(bool bluetoothControlGranted READ bluetoothControlGranted CONSTANT)
  Q_PROPERTY(bool powerControlGranted READ powerControlGranted CONSTANT)

public:
  SystemStatusController(AudioApplet::AudioAppletController *audio,
                         BluetoothApplet::BluetoothAppletController *bluetooth,
                         PowerApplet::PowerAppletController *power,
                         SystemStatusGrants grants, QObject *parent = nullptr);

  // Rows: {id, label, phase, summary, iconName, accessibleName,
  // accessibleDescription, available, attention}.
  [[nodiscard]] QVariantList laneRows() const;
  [[nodiscard]] int availableLaneCount() const noexcept
  {
    return m_model.availableLaneCount;
  }
  [[nodiscard]] int laneCount() const noexcept
  {
    return static_cast<int>(m_model.lanes.size());
  }
  [[nodiscard]] bool anyLaneAvailable() const noexcept
  {
    return m_model.availableLaneCount > 0;
  }
  [[nodiscard]] QString accessibleName() const { return m_model.accessibleName; }
  [[nodiscard]] QString accessibleDescription() const
  {
    return m_model.accessibleDescription;
  }
  // Null when the lane is not granted, even if the facade exists.
  [[nodiscard]] QObject *audio() const noexcept;
  [[nodiscard]] QObject *bluetooth() const noexcept;
  [[nodiscard]] QObject *power() const noexcept;
  [[nodiscard]] bool audioControlGranted() const noexcept
  {
    return m_grants.audioRead && m_grants.audioControl;
  }
  [[nodiscard]] bool bluetoothControlGranted() const noexcept
  {
    return m_grants.bluetoothRead && m_grants.bluetoothControl;
  }
  [[nodiscard]] bool powerControlGranted() const noexcept
  {
    return m_grants.powerRead && m_grants.powerControl;
  }
  [[nodiscard]] const SystemStatusModel &model() const noexcept { return m_model; }

Q_SIGNALS:
  void stateChanged();

private:
  void reproject();
  [[nodiscard]] StatusLaneInput audioLane() const;
  [[nodiscard]] StatusLaneInput bluetoothLane() const;
  [[nodiscard]] StatusLaneInput powerLane() const;

  AudioApplet::AudioAppletController *m_audio = nullptr;
  BluetoothApplet::BluetoothAppletController *m_bluetooth = nullptr;
  PowerApplet::PowerAppletController *m_power = nullptr;
  SystemStatusGrants m_grants;
  SystemStatusModel m_model;
};

} // namespace QindaQt::Shell::DesktopControls
