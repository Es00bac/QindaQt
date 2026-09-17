// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applet_host/capability_policy.h"
#include "qindaqt/profiles/layout_profile.h"

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace QindaQt::Applets {
class ManifestCatalog;
}

namespace QindaQt::AppletRuntime {

class BuiltinAppletRegistry;

enum class AppletResolutionStatus {
    Ready,
    MissingManifest,
    PlacementRejected,
    HostRejected,
    SandboxUnavailable,
    ImplementationUnavailable,
    PolicyRejected,
};

struct ResolvedAppletInstance {
    Profiles::AppletSpec instance;
    QString displayName;
    QString entryPoint;
    QStringList grantedCapabilities;
    AppletHost::HostMode hostMode = AppletHost::HostMode::Rejected;
    AppletResolutionStatus status = AppletResolutionStatus::MissingManifest;
    QString diagnostic;
    // AGENT-CONTRACT: the resolved manifest's declared `sizing.mainAxis.minimum`,
    // republished as `runtime.mainAxisMinimum` so the panel's zone budget can
    // honour it. The shell reserves the sum of a zone's applet minimums before
    // giving a greedy neighbour the rest (ADR-0188), which is the only reason a
    // declared minimum is more than documentation. Zero unless a manifest
    // resolved Ready.
    //
    // AGENT-GUARD: this member is last because several returns above build the
    // struct positionally; a new field inserted earlier silently shifts
    // host mode and status into the wrong slots.
    int mainAxisMinimum = 0;

    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] QVariantMap toVariantMap() const;
};

class AppletInstanceResolver final {
public:
    // AGENT-CONTRACT: this resolves only audited first-party packages.
    // Third-party identity and sandbox launch belong to an installed-package layer.
    [[nodiscard]] static ResolvedAppletInstance resolveBuiltin(
        const Profiles::AppletSpec &instance,
        Profiles::Edge panelEdge,
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        const BuiltinAppletRegistry &registry);

    // Desktop-zone instances (ADR-0125) resolve against the desktop surface:
    // the manifest must declare the desktop zone and no panel edge applies.
    [[nodiscard]] static ResolvedAppletInstance resolveDesktopBuiltin(
        const Profiles::AppletSpec &instance,
        const Applets::ManifestCatalog &catalog,
        const AppletHost::CapabilityPolicy &policy,
        const BuiltinAppletRegistry &registry);
};

[[nodiscard]] QString toString(AppletResolutionStatus value);

} // namespace QindaQt::AppletRuntime
