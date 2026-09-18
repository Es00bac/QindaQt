// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/mic_mute_key_controller.h"

namespace QindaQt::Session::DesktopControls {

MicMuteKeyController::MicMuteKeyController(Audio::AudioClient &client,
                                           QObject *parent)
    : QObject(parent), m_client(client)
{
    connect(&m_client, &Audio::AudioClient::operationCompleted, this,
            [this](quint64, const Audio::OperationResult &result) {
                if (result.kind != Audio::OperationKind::SetMute) {
                    return;
                }
                if (result.status == Audio::OperationStatus::Succeeded) {
                    return;
                }
                // An uncertain result is not failure; the next snapshot and
                // key press reconcile truth without replaying anything here.
                if (result.status == Audio::OperationStatus::Uncertain) {
                    return;
                }
                Q_EMIT micMuteUnavailable(result.reasonCode);
            });
}

MicMuteKeyController::~MicMuteKeyController() = default;

void MicMuteKeyController::toggleMicMute()
{
    const auto *device = defaultInputDevice();
    if (device == nullptr) {
        Q_EMIT micMuteUnavailable(QStringLiteral("no-default-input"));
        return;
    }
    if (!device->canSetMute || !device->muteKnown) {
        Q_EMIT micMuteUnavailable(QStringLiteral("mute-unsupported"));
        return;
    }
    const bool muted = !device->muted;
    if (m_client.setMute(device->handle, muted) == 0U) {
        // Only request-id exhaustion lands here; the busy and no-owner cases
        // complete asynchronously and report through operationCompleted.
        Q_EMIT micMuteUnavailable(QStringLiteral("client-exhausted"));
        return;
    }
    Q_EMIT micMuteFeedbackRequested(muted);
}

const Audio::Device *MicMuteKeyController::defaultInputDevice() const
{
    if (!m_client.hasSnapshot()) {
        return nullptr;
    }
    const auto &snapshot = m_client.snapshot();
    if (!snapshot.defaultInput.isValid()) {
        return nullptr;
    }
    for (const auto &device : snapshot.inputs) {
        if (device.handle == snapshot.defaultInput) {
            return &device;
        }
    }
    return nullptr;
}

} // namespace QindaQt::Session::DesktopControls
