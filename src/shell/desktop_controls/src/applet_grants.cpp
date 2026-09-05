// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/desktop_controls/applet_grants.h"

#include "qindaqt/applet_host/capability_policy.h"
#include "qindaqt/applet_host/host_selection.h"
#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"

namespace QindaQt::Shell::DesktopControls {

AuditedGrants evaluateAuditedGrants(const Applets::ManifestCatalog &catalog,
                                    const AppletHost::CapabilityPolicy &policy,
                                    const AppletRuntime::BuiltinAppletRegistry &registry,
                                    const QString &manifestId)
{
  AuditedGrants grants;
  const auto *manifest = catalog.findById(manifestId);
  if (manifest == nullptr) {
    grants.diagnostic = QStringLiteral("missing-manifest");
    return grants;
  }
  const AppletHost::PackageIdentity package{
      manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
  const auto host = AppletHost::HostSelector::select(*manifest, package);
  if (host.mode != AppletHost::HostMode::InProcessAuditedBuiltin) {
    grants.diagnostic = QStringLiteral("host-rejected");
    return grants;
  }
  if (!registry.contains(manifest->entryPoint.value)) {
    grants.diagnostic = QStringLiteral("implementation-unavailable");
    return grants;
  }
  const auto evaluated = policy.evaluate(*manifest, package);
  if (!evaluated.ok) {
    grants.diagnostic = QStringLiteral("policy-rejected");
    return grants;
  }
  grants.resolved = true;
  for (const auto &decision : evaluated.decisions) {
    if (decision.granted()) {
      grants.granted.append(decision.capability);
    }
  }
  return grants;
}

} // namespace QindaQt::Shell::DesktopControls
