// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/volume_key_controller.h"

#include <QtGlobal>

namespace QindaQt::Session::DesktopControls {
namespace {

double clampedLevel(double level) noexcept
{
    if (!(level > 0.0)) {
        return 0.0;
    }
    return level > 1.0 ? 1.0 : level;
}

int percentOf(double level) noexcept
{
    return qRound(clampedLevel(level) * 100.0);
}

} // namespace

VolumeKeyController::VolumeKeyController(Audio::AudioClient &client,
                                         QObject *parent)
    : QObject(parent), m_client(client)
{
    connect(&m_client, &Audio::AudioClient::operationCompleted, this,
            [this](quint64, const Audio::OperationResult &result) {
                if (result.kind != Audio::OperationKind::SetVolume
                    && result.kind != Audio::OperationKind::SetMute) {
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
                Q_EMIT volumeUnavailable(result.reasonCode);
            });
}

VolumeKeyController::~VolumeKeyController() = default;

void VolumeKeyController::raiseVolume()
{
    submitStep(step());
}

void VolumeKeyController::lowerVolume()
{
    submitStep(-step());
}

void VolumeKeyController::toggleMute()
{
    const auto *device = defaultOutputDevice();
    if (device == nullptr) {
        Q_EMIT volumeUnavailable(QStringLiteral("no-default-output"));
        return;
    }
    if (!device->canSetMute || !device->muteKnown) {
        Q_EMIT volumeUnavailable(QStringLiteral("mute-unsupported"));
        return;
    }
    const bool muted = !device->muted;
    const int percent = device->volumeKnown ? percentOf(device->volume) : 0;
    if (m_client.setMute(device->handle, muted) == 0U) {
        // Only request-id exhaustion lands here; the busy and no-owner cases
        // complete asynchronously and report through operationCompleted.
        Q_EMIT volumeUnavailable(QStringLiteral("client-exhausted"));
        return;
    }
    Q_EMIT volumeFeedbackRequested(percent, muted);
}

void VolumeKeyController::submitStep(double delta)
{
    const auto *device = defaultOutputDevice();
    if (device == nullptr) {
        Q_EMIT volumeUnavailable(QStringLiteral("no-default-output"));
        return;
    }
    if (!device->canSetVolume) {
        Q_EMIT volumeUnavailable(QStringLiteral("volume-unsupported"));
        return;
    }
    // AGENT-NOTE: an unknown current level starts from silence for raises and
    // stays at the floor for lowers, so the first visible feedback always
    // matches a level the device can actually reach.
    const double current = device->volumeKnown ? clampedLevel(device->volume) : 0.0;
    const double next = clampedLevel(current + delta);
    if (m_client.setVolume(device->handle, next) == 0U) {
        Q_EMIT volumeUnavailable(QStringLiteral("client-exhausted"));
        return;
    }
    Q_EMIT volumeFeedbackRequested(percentOf(next), device->muteKnown && device->muted);}

const Audio::Device *VolumeKeyController::defaultOutputDevice() const
{
    if (!m_client.hasSnapshot()) {
        return nullptr;
    }
    const auto &snapshot = m_client.snapshot();
    if (!snapshot.defaultOutput.isValid()) {
        return nullptr;
    }
    for (const auto &device : snapshot.outputs) {
        if (device.handle == snapshot.defaultOutput) {
            return &device;
        }
    }
    return nullptr;
}

} // namespace QindaQt::Session::DesktopControls
