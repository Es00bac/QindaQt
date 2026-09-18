// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <qindaqt/services/audio_client/audio_client.h>

#include <QObject>

namespace QindaQt::Session::DesktopControls {

// XF86AudioMicMute over the public AudioClient. Mirrors
// VolumeKeyController::toggleMute() exactly, against the default INPUT
// device instead of the default output.
class MicMuteKeyController final : public QObject {
    Q_OBJECT

public:
    explicit MicMuteKeyController(Audio::AudioClient &client,
                                  QObject *parent = nullptr);
    ~MicMuteKeyController() override;

    MicMuteKeyController(const MicMuteKeyController &) = delete;
    MicMuteKeyController &operator=(const MicMuteKeyController &) = delete;

    void toggleMicMute();

Q_SIGNALS:
    // `muted` is the intended mute state after this trigger.
    void micMuteFeedbackRequested(bool muted);
    void micMuteUnavailable(const QString &reasonCode);

private:
    [[nodiscard]] const Audio::Device *defaultInputDevice() const;

    Audio::AudioClient &m_client;
};

} // namespace QindaQt::Session::DesktopControls
