// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <qindaqt/services/voice_client/voice_client.h>
#include <qindaqt/shell/voice_applet/voice_client_interface.h>

namespace QindaQt::Shell::VoiceApplet {

// Binds the seam to a live org.qindaqt.Voice1 consumer.
//
// AGENT-CONTRACT: this class is the only place the applet stack names
// Services::Voice::VoiceClient. It forwards and translates; it holds no state
// of its own, so there is nothing here that can disagree with the client.
class VoiceServiceClientAdapter final : public VoiceClientInterface {
    Q_OBJECT
public:
    explicit VoiceServiceClientAdapter(Services::Voice::VoiceClient *client,
                                       QObject *parent = nullptr);

    [[nodiscard]] Services::Voice::ClientState clientState() const noexcept override;
    [[nodiscard]] QString reasonCode() const override;
    [[nodiscard]] bool hasSnapshot() const noexcept override;
    [[nodiscard]] Services::Voice::Snapshot snapshot() const override;
    [[nodiscard]] quint32 levelPercent() const noexcept override;
    void refresh() override;
    [[nodiscard]] quint64 submit(Services::Voice::OperationKind kind,
                                 const QString &providerId, bool enable) override;

private:
    Services::Voice::VoiceClient *m_client = nullptr;
};

} // namespace QindaQt::Shell::VoiceApplet
