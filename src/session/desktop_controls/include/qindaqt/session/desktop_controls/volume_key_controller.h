// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/audio_client/audio_client.h>

#include <QObject>

namespace QindaQt::Session::DesktopControls {

// Media-key volume and mute over the public AudioClient. Every trigger
// resolves the current default output from the latest validated snapshot,
// applies one bounded step, and submits through the serialized client path;
// it never replays a timed-out operation. Feedback is optimistic at trigger
// time so the user sees the intended level immediately; a rejected operation
// reports honest unavailability instead.
class VolumeKeyController final : public QObject {
    Q_OBJECT

public:
    explicit VolumeKeyController(Audio::AudioClient &client,
                                 QObject *parent = nullptr);
    ~VolumeKeyController() override;

    VolumeKeyController(const VolumeKeyController &) = delete;
    VolumeKeyController &operator=(const VolumeKeyController &) = delete;

    void raiseVolume();
    void lowerVolume();
    void toggleMute();

    [[nodiscard]] static constexpr double step() noexcept { return 0.05; }

Q_SIGNALS:
    // `percent` is the intended level in 0..100 after this trigger; `muted`
    // is the intended mute state after this trigger.
    void volumeFeedbackRequested(int percent, bool muted);
    void volumeUnavailable(const QString &reasonCode);

private:
    void submitStep(double delta);
    void submitMute(bool muted);
    [[nodiscard]] const Audio::Device *defaultOutputDevice() const;

    Audio::AudioClient &m_client;
};

} // namespace QindaQt::Session::DesktopControls
