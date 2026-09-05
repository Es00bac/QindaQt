// SPDX-License-Identifier: GPL-3.0-or-later

#include "globalmenuappletcomposition.h"

#include "qindaqt/applet_host/capability_policy_loader.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/shell/global_menu/applet/globalmenuappletaccess.h"
#include "qindaqt/shell/global_menu/composition/announced_menu_address_source.h"
#include "qindaqt/shell/global_menu/composition/global_menu_transport_coordinator.h"
#include "qindaqt/shell/global_menu/composition/registrar_window_id_source.h"
#include "qindaqt/shell/global_menu/ownership/active_window_source.h"
#include "qindaqt/shell/global_menu/registrar/appmenu_registrar.h"
#include "qindaqt/shell_window_actions_client/shell_window_actions_client.h"

#include <QDBusObjectPath>
#include <QUuid>

#include <utility>

namespace QindaQt::Shell {
namespace {

bool globalMenuReadGranted(const Applets::ManifestCatalog &catalog,
                           const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("global-menu"));
    if (manifest == nullptr) {
        return false;
    }
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    const auto registry = AppletRuntime::BuiltinAppletRegistry::firstParty();
    if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin
        || !registry.contains(manifest->entryPoint.value)) {
        return false;
    }
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) {
        return false;
    }
    for (const auto &decision : evaluated.decisions) {
        if (decision.capability == Applets::Capability::GlobalMenuRead) {
            return decision.granted();
        }
    }
    return false;
}

QString registrarFailureCode(GlobalMenu::Registrar::RegistrarStartStatus status)
{
    using Status = GlobalMenu::Registrar::RegistrarStartStatus;
    switch (status) {
    case Status::Started:
        return {};
    case Status::NameAlreadyOwned:
        return QStringLiteral("registrar-name-owned");
    case Status::InvalidConnection:
        return QStringLiteral("session-bus-unavailable");
    case Status::ObjectRegistrationFailed:
        return QStringLiteral("registrar-object-failed");
    case Status::NameRegistrationFailed:
        return QStringLiteral("registrar-name-failed");
    }
    return QStringLiteral("registrar-start-failed");
}

} // namespace

class GlobalMenuIdentityAdapter final
    : public GlobalMenu::Ownership::ActiveWindowSource,
      public GlobalMenu::Composition::RegistrarWindowIdSource,
      public GlobalMenu::Composition::AnnouncedMenuAddressSource
{
public:
    explicit GlobalMenuIdentityAdapter(
        const ShellWindowActionsClient::ShellWindowActionsClient &client)
        : m_client(client)
    {
    }

    [[nodiscard]] std::optional<GlobalMenu::Ownership::ActiveWindowObservation>
    activeWindow() const override
    {
        const auto facts = currentFacts();
        if (!facts) {
            return std::nullopt;
        }
        return GlobalMenu::Ownership::ActiveWindowObservation{
            .window = facts->first, .focusGeneration = facts->second};
    }

    [[nodiscard]] std::optional<quint32> registrarWindowIdFor(
        const GlobalMenu::Ownership::WindowIdentity &window) const override
    {
        const auto snapshot = currentSnapshot(window);
        return snapshot ? snapshot->activeWindow->appMenuWindowId : std::nullopt;
    }

    [[nodiscard]] std::optional<GlobalMenu::Composition::AnnouncedMenuAddress>
    announcedMenuFor(
        const GlobalMenu::Ownership::WindowIdentity &window) const override
    {
        const auto snapshot = currentSnapshot(window);
        if (!snapshot || snapshot->activeWindow->appMenuWindowId
            || !snapshot->activeWindow->appMenuServiceName
            || !snapshot->activeWindow->appMenuObjectPath) {
            return std::nullopt;
        }
        return GlobalMenu::Composition::AnnouncedMenuAddress{
            .serviceName = *snapshot->activeWindow->appMenuServiceName,
            .objectPath = *snapshot->activeWindow->appMenuObjectPath};
    }

private:
    using IdentitySnapshot = Compositor::ShellWindowIdentitySnapshot;

    [[nodiscard]] const IdentitySnapshot *currentSnapshot(
        const GlobalMenu::Ownership::WindowIdentity &expected) const
    {
        const auto &snapshot = m_client.identitySnapshot();
        if (!m_client.identityAvailable() || !snapshot || !snapshot->activeWindow
            || !snapshot->activeWindow->processId) {
            return nullptr;
        }
        const QUuid windowId = QUuid::fromString(
            snapshot->activeWindow->windowId);
        if (windowId.isNull() || windowId != expected.windowId
            || *snapshot->activeWindow->processId != expected.processId) {
            return nullptr;
        }
        return &*snapshot;
    }

    [[nodiscard]] std::optional<
        std::pair<GlobalMenu::Ownership::WindowIdentity, quint64>>
    currentFacts() const
    {
        const auto &snapshot = m_client.identitySnapshot();
        if (!m_client.identityAvailable() || !snapshot || !snapshot->activeWindow
            || !snapshot->activeWindow->processId || snapshot->revision == 0) {
            return std::nullopt;
        }
        const QUuid windowId = QUuid::fromString(
            snapshot->activeWindow->windowId);
        const qint64 processId = *snapshot->activeWindow->processId;
        if (windowId.isNull() || processId <= 0) {
            return std::nullopt;
        }
        return std::pair{GlobalMenu::Ownership::WindowIdentity{
                             .windowId = windowId, .processId = processId},
                         snapshot->revision};
    }

    const ShellWindowActionsClient::ShellWindowActionsClient &m_client;
};

GlobalMenuAppletComposition::GlobalMenuAppletComposition(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    QDBusConnection sessionBus,
    ShellWindowActionsClient::ShellWindowActionsClient &windowActions)
    : m_sessionBus(std::move(sessionBus))
    , m_windowActions(windowActions)
    , m_granted(globalMenuReadGranted(catalog, policy))
    , m_access(std::make_unique<GlobalMenu::GlobalMenuAppletAccess>())
    , m_identity(std::make_unique<GlobalMenuIdentityAdapter>(windowActions))
    , m_registrar(std::make_unique<GlobalMenu::Registrar::AppMenuRegistrar>(
          m_sessionBus))
{
}

GlobalMenuAppletComposition::~GlobalMenuAppletComposition()
{
    stop();
}

void GlobalMenuAppletComposition::start()
{
    stop();
    if (!m_granted) {
        m_status = GlobalMenuRuntimeStatus::Unavailable;
        m_reasonCode = QStringLiteral("global-menu-read-denied");
        m_access->publishUnavailable();
        return;
    }
    const auto registrarStatus = m_registrar->start();
    if (registrarStatus != GlobalMenu::Registrar::RegistrarStartStatus::Started) {
        m_status = GlobalMenuRuntimeStatus::Degraded;
        m_reasonCode = registrarFailureCode(registrarStatus);
        m_access->publishDegraded(m_reasonCode);
        return;
    }
    m_coordinator = std::make_unique<
        GlobalMenu::Composition::GlobalMenuTransportCoordinator>(
            m_sessionBus, *m_identity, *m_identity, *m_identity,
            *m_registrar->registry(), *m_access);
    QObject::connect(
        m_coordinator.get(),
        &GlobalMenu::Composition::GlobalMenuTransportCoordinator::hostedMenuChanged,
        m_registrar.get(),
        [this](const QString &provider, const QString &path, bool hosted) {
            m_registrar->setHostedMenu(provider, path, hosted);
        });
    QObject::connect(m_access.get(),
                     &GlobalMenu::GlobalMenuAppletAccess::rendererPresentChanged,
                     m_registrar.get(), [this] {
                         if (!m_access->rendererPresent()) {
                             m_registrar->clearHostedMenus();
                         }
                     });
    QObject::connect(&m_windowActions,
                     &ShellWindowActionsClient::ShellWindowActionsClient::identityChanged,
                     m_coordinator.get(),
                     &GlobalMenu::Composition::GlobalMenuTransportCoordinator::refreshFocus);
    m_status = GlobalMenuRuntimeStatus::Ready;
    m_reasonCode.clear();
    m_coordinator->refreshFocus();
}

void GlobalMenuAppletComposition::stop()
{
    m_coordinator.reset();
    m_registrar->stop();
    m_access->publishUnavailable();
    m_status = GlobalMenuRuntimeStatus::Unavailable;
    m_reasonCode.clear();
}

GlobalMenu::GlobalMenuAppletAccess *
GlobalMenuAppletComposition::access() const noexcept
{
    return m_access.get();
}

GlobalMenuRuntimeStatus GlobalMenuAppletComposition::status() const noexcept
{
    return m_status;
}

const QString &GlobalMenuAppletComposition::reasonCode() const noexcept
{
    return m_reasonCode;
}

} // namespace QindaQt::Shell
