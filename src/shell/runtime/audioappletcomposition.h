// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::Audio {
class AudioClient;
class QtAudioTransport;
}

namespace QindaQt::Shell::AudioApplet {
class AudioAppletController;
}

namespace QindaQt::Shell {

// Shell-private ownership boundary for the public Audio1 client and the
// purpose-specific built-in facade. The audited manifest/registry/policy gates
// are evaluated once, independently of panel-window reconstruction.
class AudioAppletComposition final
{
public:
    AudioAppletComposition(const Applets::ManifestCatalog &catalog,
                           const AppletHost::CapabilityPolicy &policy);
    ~AudioAppletComposition();

    AudioAppletComposition(const AudioAppletComposition &) = delete;
    AudioAppletComposition &operator=(const AudioAppletComposition &) = delete;

    [[nodiscard]] AudioApplet::AudioAppletController *access() const noexcept;

private:
    // AGENT-CONTRACT: reverse member destruction is controller -> client ->
    // transport. Shell teardown destroys panel windows first; the destructor
    // then stops the client only after the controller is gone, so no
    // completion can reach a destroyed facade.
    std::unique_ptr<Audio::QtAudioTransport> m_transport;
    std::unique_ptr<Audio::AudioClient> m_client;
    std::unique_ptr<AudioApplet::AudioAppletController> m_access;
};

} // namespace QindaQt::Shell
