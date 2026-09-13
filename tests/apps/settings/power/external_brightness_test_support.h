// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "fake_display_transport.h"

#include <qindaqt/apps/settings_power/external_display_brightness_model.h>
#include <qindaqt/services/display_client/client.h>

#include <QtCore/QVariantMap>

namespace QindaQt::Apps::SettingsPower::TestSupport::External {

using FakeDisplayTransport = DisplayClient::TestSupport::FakeDisplayTransport;

inline const QString kOwner = QStringLiteral(":1.90");
inline const QString kEpoch = QStringLiteral("epoch-a");

inline Display::Output output(const QString &connector, const QString &label,
                              const quint32 priority, const bool internal = false) {
  const Display::Mode mode{.id = QStringLiteral("1920x1080@60"),
                           .pixelSize = QSize(1920, 1080),
                           .refreshMilliHertz = 60'000,
                           .preferred = true};
  return {.stableId = QStringLiteral("conn:%1").arg(connector),
          .connectorName = connector,
          .runtimeCompositorUuid = QStringLiteral("uuid-%1").arg(connector),
          .label = label,
          .manufacturer = QStringLiteral("QIN"),
          .model = QStringLiteral("Panel"),
          .physicalSizeMillimeters = QSize(600, 340),
          .hasSerial = false,
          .internal = internal,
          .ambiguousIdentity = false,
          .enabled = true,
          .primary = priority == 1,
          .modeId = mode.id,
          .position = QPoint(static_cast<int>(priority) * 2'000, 0),
          .logicalSize = QSize(1920, 1080),
          .scale = 1.0,
          .transform = Display::Transform::Normal,
          .priority = priority,
          .replicationSourceStableId = {},
          .modes = {mode},
          .wireValid = true};
}

// Two adjustable external monitors, the internal panel that Power1 owns, one
// external output without brightness control, and one disabled output.
inline Display::Snapshot topology(const quint64 revision = 4) {
  Display::Output spare = output(QStringLiteral("DP-3"), QStringLiteral("Spare"), 0);
  spare.enabled = false;
  spare.position = QPoint();
  return {.protocolVersion = 1,
          .serviceEpoch = kEpoch,
          .revision = revision,
          .liveFingerprint = QByteArray(32, '\0'),
          .outputs = {output(QStringLiteral("DP-1"), QStringLiteral("Studio monitor"), 1),
                      output(QStringLiteral("eDP-1"), QStringLiteral("Built-in panel"), 2, true),
                      output(QStringLiteral("HDMI-A-1"), QStringLiteral("Projector"), 3),
                      output(QStringLiteral("DP-2"), QStringLiteral("Office TV"), 4),
                      spare},
          .transactions = {},
          .wireValid = true};
}

inline Display::BrightnessSnapshot brightness(const Display::Snapshot &joinedTopology,
                                              const quint64 revision = 9) {
  Display::BrightnessSnapshot value{.protocolVersion = 1,
                                    .serviceEpoch = joinedTopology.serviceEpoch,
                                    .topologyRevision = joinedTopology.revision,
                                    .revision = revision,
                                    .outputs = {},
                                    .wireValid = true};
  for (const Display::Output &item : joinedTopology.outputs) {
    Display::OutputBrightness row{.stableId = item.stableId,
                                  .capable = item.enabled,
                                  .observed = item.enabled,
                                  .value = 0};
    if (item.connectorName == QStringLiteral("DP-1")) row.value = 6'000;
    if (item.connectorName == QStringLiteral("eDP-1")) row.value = 3'000;
    if (item.connectorName == QStringLiteral("HDMI-A-1")) row.value = 2'500;
    if (item.connectorName == QStringLiteral("DP-2")) {
      // KWin reports a level for outputs it cannot adjust.
      row.capable = false;
      row.value = 10'000;
    }
    value.outputs.append(row);
  }
  return value;
}

inline Display::BrightnessSnapshot withValue(Display::BrightnessSnapshot value,
                                             const QString &connector,
                                             const quint32 level,
                                             const quint64 revision) {
  for (Display::OutputBrightness &row : value.outputs)
    if (row.stableId == QStringLiteral("conn:%1").arg(connector)) row.value = level;
  value.revision = revision;
  return value;
}

inline Display::OperationResult immediate(const Display::OperationStatus status,
                                          const quint64 initiating,
                                          const quint64 observed,
                                          const QString &diagnostic = {},
                                          const Display::ErrorCode error =
                                              Display::ErrorCode::None) {
  return {.kind = Display::OperationKind::ImmediatePolicy,
          .status = status,
          .error = error,
          .initiatingEpoch = kEpoch,
          .initiatingRevision = initiating,
          .observedRevision = observed,
          .transactionId = {},
          .diagnostic = diagnostic,
          .wireValid = true};
}

// One fake Display1 transport, the public client, and the route model.
struct Route {
  FakeDisplayTransport transport;
  DisplayClient::Client client{&transport};
  ExternalDisplayBrightnessModel model{client};

  void publish(const Display::Snapshot &topologyValue,
               const Display::BrightnessSnapshot &brightnessValue) {
    client.start();
    transport.publishOwner(kOwner);
    transport.replySnapshot(transport.fetches.constLast(), topologyValue);
    transport.replyBrightness(transport.brightnessFetches.constLast(), brightnessValue);
  }

  // Changed may repeat the topology revision: a complete read, then brightness.
  void republish(const Display::Snapshot &topologyValue,
                 const Display::BrightnessSnapshot &brightnessValue) {
    transport.publishInvalidation(kOwner, topologyValue.serviceEpoch,
                                  topologyValue.revision);
    transport.replySnapshot(transport.fetches.constLast(), topologyValue);
    transport.replyBrightness(transport.brightnessFetches.constLast(), brightnessValue);
  }

  void finish(const qsizetype index, const Display::OperationResult &result) {
    const auto &submission = transport.brightnessSubmissions.at(index);
    transport.replyOperationAs(submission.owner, submission.requestId, result);
  }

  [[nodiscard]] QVariantMap row(const qsizetype index) const {
    return model.rows().at(index).toMap();
  }
};

} // namespace QindaQt::Apps::SettingsPower::TestSupport::External
