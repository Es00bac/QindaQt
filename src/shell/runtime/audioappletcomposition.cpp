// SPDX-License-Identifier: GPL-3.0-or-later

#include "audioappletcomposition.h"

#include "audio_applet_controller.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/audio_client/audio_client.h"
#include "qindaqt/services/audio_client/qt_audio_transport.h"

#include <QtDBus/QDBusConnection>

namespace QindaQt::Shell
{
namespace
{

struct AudioAppletGrants {
    bool read = false;
    bool control = false;
};

AudioAppletGrants audioAppletGrants(const Applets::ManifestCatalog &catalog,
                                    const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("audio"));
    if (manifest == nullptr) {
        return {};
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return {};
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return {};
    }
    AudioAppletGrants grants;
    for (const auto &decision : evaluated.decisions) {
        if (!decision.granted()) {
            continue;
        }
        if (decision.capability == Applets::Capability::AudioRead) {
            grants.read = true;
        } else if (decision.capability == Applets::Capability::AudioControl) {
            grants.control = true;
        }
    }
    return grants;
}

} // namespace

AudioAppletComposition::AudioAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy)
{
    const AudioAppletGrants grants = audioAppletGrants(catalog, policy);
    m_transport = std::make_unique<Audio::QtAudioTransport>(
        QDBusConnection::sessionBus());
    m_client = std::make_unique<Audio::AudioClient>(m_transport.get());
    m_access = std::make_unique<AudioApplet::AudioAppletController>(
        m_client.get(), grants.read, grants.control);
    if (grants.read) {
        m_client->start();
    }
}

AudioAppletComposition::~AudioAppletComposition()
{
    m_client->stop();
}

AudioApplet::AudioAppletController *AudioAppletComposition::access() const noexcept
{
    return m_access.get();
}

} // namespace QindaQt::Shell
