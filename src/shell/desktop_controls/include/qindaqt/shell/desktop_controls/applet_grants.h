// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <qindaqt/applets/manifest_types.h>

#include <QString>
#include <QVector>

namespace QindaQt::Applets {
class ManifestCatalog;
}
namespace QindaQt::AppletHost {
class CapabilityPolicy;
}
namespace QindaQt::AppletRuntime {
class BuiltinAppletRegistry;
}

namespace QindaQt::Shell::DesktopControls {

// Result of the shared audited-built-in gate: manifest lookup, in-process host
// selection, compiled registry membership, then policy evaluation. `granted`
// carries only affirmative decisions; an unresolved manifest grants nothing.
struct AuditedGrants {
  bool resolved = false;
  QString diagnostic;
  QVector<Applets::Capability> granted;

  [[nodiscard]] bool has(Applets::Capability capability) const
  {
    return resolved && granted.contains(capability);
  }
};

// AGENT-CONTRACT: this mirrors the per-applet gate in every shell composition
// (power, launcher, task list, ...). It exists so the desktop controls
// evaluate twelve manifests through one implementation instead of twelve
// hand-copied blocks; the ordering of the five gates must match
// AppletInstanceResolver::resolveBuiltin.
[[nodiscard]] AuditedGrants evaluateAuditedGrants(
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    const AppletRuntime::BuiltinAppletRegistry &registry,
    const QString &manifestId);

} // namespace QindaQt::Shell::DesktopControls
