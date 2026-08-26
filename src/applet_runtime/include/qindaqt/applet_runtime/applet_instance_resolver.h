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
};

[[nodiscard]] QString toString(AppletResolutionStatus value);

} // namespace QindaQt::AppletRuntime
