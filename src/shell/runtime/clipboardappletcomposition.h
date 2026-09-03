// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusConnection>

#include <memory>

namespace QindaQt::AppletHost {
class CapabilityPolicy;
}

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::Services::Clipboard {
class ClipboardClient;
class ClipboardTransport;
class QtClipboardTransport;
}

namespace QindaQt::Services::SettingsClient {
class SettingsClient;
}

namespace QindaQt::ShellClipboardApplet {
class ClipboardAppletController;
class ClipboardClientInterface;
}

namespace QindaQt::Shell {

// Shell-private production boundary for the Clipboard1 and Settings1 clients.
// The controller sees only its least-authority interface; raw D-Bus and
// consent provenance remain owned by this composition.
class ClipboardAppletComposition final
{
public:
    ClipboardAppletComposition(
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        Services::SettingsClient::SettingsClient &settingsClient,
        const QDBusConnection &sessionBus);
    ClipboardAppletComposition(
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        Services::SettingsClient::SettingsClient &settingsClient,
        Services::Clipboard::ClipboardTransport &transport);
    ~ClipboardAppletComposition();

    ClipboardAppletComposition(const ClipboardAppletComposition &) = delete;
    ClipboardAppletComposition &operator=(const ClipboardAppletComposition &) = delete;

    [[nodiscard]] ShellClipboardApplet::ClipboardAppletController *access() const noexcept;

private:
    void compose(const Applets::ManifestCatalog &catalog,
                 const AppletHost::CapabilityPolicy &policy,
                 Services::SettingsClient::SettingsClient &settingsClient,
                 Services::Clipboard::ClipboardTransport &transport);

    // AGENT-CONTRACT: reverse destruction is controller -> bridge -> client
    // -> owned transport. The injected transport and borrowed Settings1
    // client must outlive this object; the shell removes panel windows first.
    std::unique_ptr<Services::Clipboard::QtClipboardTransport> m_ownedTransport;
    std::unique_ptr<Services::Clipboard::ClipboardClient> m_client;
    std::unique_ptr<ShellClipboardApplet::ClipboardClientInterface> m_bridge;
    std::unique_ptr<ShellClipboardApplet::ClipboardAppletController> m_access;
};

} // namespace QindaQt::Shell
