// SPDX-License-Identifier: GPL-3.0-or-later

#include "linkappletcomposition.h"

#include "link_applet_controller.h"
#include "launcher_applet_controller.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/services/link_client/link_client.h"
#include "qindaqt/services/link_client/qt_link_transport.h"

#include <QtCore/QDebug>
#include <QtDBus/QDBusConnection>

namespace QindaQt::Shell
{
namespace
{

struct Grants {
    bool read = false;
    bool control = false;
};

Grants phoneGrants(const Applets::ManifestCatalog &catalog,
                   const AppletHost::CapabilityPolicy &policy)
{
    Grants out;
    const auto *manifest = catalog.findById(QStringLiteral("link"));
    if (manifest == nullptr) {
        return out;
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return out;
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return out;
    }
    for (const auto &decision : evaluated.decisions) {
        if (!decision.granted()) {
            continue;
        }
        if (decision.capability == Applets::Capability::PhoneRead) {
            out.read = true;
        } else if (decision.capability == Applets::Capability::PhoneControl) {
            out.control = true;
        }
    }
    return out;
}

} // namespace

LinkAppletComposition::LinkAppletComposition(const Applets::ManifestCatalog &catalog,
                                             const AppletHost::CapabilityPolicy &policy)
{
    const Grants grants = phoneGrants(catalog, policy);
    // AGENT-GUARD: a denied read grant constructs no transport and no client
    // rather than an unstarted one, so there is no live D-Bus watch to
    // forget to stop. Control without read is meaningless and is dropped.
    if (grants.read) {
        // AGENT-NOTE: the service may start after the shell (or after the
        // phone pairs). QtLinkTransport watches the bus-name owner and the
        // client re-requests Devices() on every availability transition, so
        // nothing here reads the service once.
        m_transport = std::make_unique<Link::QtLinkTransport>(QDBusConnection::sessionBus());
        m_client = std::make_unique<Link::LinkClient>(*m_transport);
    }
    m_access = std::make_unique<LinkApplet::LinkAppletController>(
        m_client.get(), grants.read, grants.read && grants.control);
    if (grants.read) {
        QString error;
        if (!m_client->start(&error)) {
            qWarning().noquote() << "QindaQt shell Link1 client unavailable:" << error;
        }
    }
}

LinkAppletComposition::~LinkAppletComposition()
{
    if (m_client) {
        m_client->stop();
    }
}

LinkApplet::LinkAppletController *LinkAppletComposition::access() const noexcept
{
    return m_access.get();
}

void LinkAppletComposition::attachLauncher(Launcher::LauncherAppletController *launcher)
{
    if (m_access == nullptr || launcher == nullptr) {
        return;
    }
    // The desktop entry is installed by the QindaLink app package; until
    // then the catalog has no such entry and the action stays hidden.
    const QString entry = QStringLiteral("org.qindaqt.Link.desktop");
    m_access->setAppLaunch(
        [launcher, entry] { return !launcher->applicationPresentation(entry).isEmpty(); },
        [launcher, entry] { return launcher->activate(entry); });
}

} // namespace QindaQt::Shell
