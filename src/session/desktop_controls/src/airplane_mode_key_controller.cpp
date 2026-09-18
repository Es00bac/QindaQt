// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/airplane_mode_key_controller.h"

#include <optional>

namespace QindaQt::Session::DesktopControls {
namespace {

std::optional<Network::Radio> findWifiRadio(const Network::Snapshot &snapshot)
{
    for (const auto &radio : snapshot.radios) {
        if (radio.kind == Network::RadioKind::Wifi && radio.present) {
            return radio;
        }
    }
    return std::nullopt;
}

} // namespace

AirplaneModeKeyController::AirplaneModeKeyController(
    Network::Client::NetworkClient &client, QObject *parent)
    : QObject(parent), m_client(client)
{
    connect(&m_client, &Network::Client::NetworkClient::operationFinished, this,
            [this](const Network::OperationResult &result) {
                if (result.kind != Network::OperationKind::SetRadio) {
                    return;
                }
                if (result.status == Network::OperationStatus::Succeeded
                    || result.status == Network::OperationStatus::Uncertain) {
                    // An uncertain result is not failure; the next snapshot
                    // and key press reconcile truth without replaying
                    // anything here.
                    return;
                }
                Q_EMIT airplaneModeUnavailable(result.reasonCode);
            });
}

AirplaneModeKeyController::~AirplaneModeKeyController() = default;

void AirplaneModeKeyController::toggleAirplaneMode()
{
    if (!m_client.model().snapshot().has_value()) {
        Q_EMIT airplaneModeUnavailable(QStringLiteral("no-snapshot"));
        return;
    }
    const std::optional<Network::Radio> wifi =
        findWifiRadio(*m_client.model().snapshot());
    if (!wifi.has_value()) {
        Q_EMIT airplaneModeUnavailable(QStringLiteral("no-wifi-radio"));
        return;
    }
    const bool newRadioEnabled = !wifi->softwareEnabled;
    QString error;
    if (!m_client.setRadio(Network::RadioKind::Wifi, newRadioEnabled, &error)) {
        Q_EMIT airplaneModeUnavailable(error);
        return;
    }
    Q_EMIT airplaneModeFeedbackRequested(!newRadioEnabled);
}

} // namespace QindaQt::Session::DesktopControls
