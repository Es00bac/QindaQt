// SPDX-License-Identifier: GPL-3.0-or-later
#include "agentusageappletcomposition.h"
#include "agent_usage_applet_controller.h"
#include <qindaqt/services/agent_usage/agent_usage_collector.h>
#include <qindaqt/applet_host/capability_policy_loader.h>
#include <qindaqt/applet_host/host_selection.h>
#include <qindaqt/applet_runtime/builtin_applet_registry.h>
#include <qindaqt/applets/manifest_catalog.h>
#include <QStandardPaths>
namespace QindaQt::Shell {
namespace {
bool readGranted(const Applets::ManifestCatalog &catalog,
                 const AppletHost::CapabilityPolicy &policy)
{
    const auto *manifest = catalog.findById(QStringLiteral("agent-usage"));
    if (!manifest || manifest->entryPoint.value != QStringLiteral("qindaqt.applets.agent-usage"))
        return false;
    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    if (AppletHost::HostSelector::select(*manifest, package).mode
            != AppletHost::HostMode::InProcessAuditedBuiltin
        || !AppletRuntime::BuiltinAppletRegistry::firstParty().contains(manifest->entryPoint.value))
        return false;
    const auto evaluated = policy.evaluate(*manifest, package);
    if (!evaluated.ok) return false;
    for (const auto &decision : evaluated.decisions)
        if (decision.capability == Applets::Capability::AgentUsageRead && decision.granted())
            return true;
    return false;
}
}
AgentUsageAppletComposition::AgentUsageAppletComposition(
    const Applets::ManifestCatalog &catalog, const AppletHost::CapabilityPolicy &policy,
    SourceFactory factory)
{
    const bool granted = readGranted(catalog, policy);
    // AGENT-GUARD: no constructor or policy-denied path starts a process or
    // reads reports. Deliberate popup/refresh actions own collection (ADR-0351).
    if (granted && factory) m_source = factory();
    else if (granted)
        m_source = std::make_unique<Services::AgentUsage::AgentUsageCollector>(
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                + QStringLiteral("/qindaqt/agent-usage/reports"),
            QStringLiteral("codex"), [] { return QDateTime::currentDateTimeUtc(); });
    m_access = std::make_unique<AgentUsageApplet::AgentUsageAppletController>(
        m_source.get(), granted);
}
AgentUsageAppletComposition::~AgentUsageAppletComposition() = default;
AgentUsageApplet::AgentUsageAppletController *AgentUsageAppletComposition::access() const noexcept
{
    return m_access.get();
}
}
