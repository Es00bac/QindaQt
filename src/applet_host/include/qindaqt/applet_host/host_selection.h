// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include "qindaqt/applets/api_version.h"
#include "qindaqt/applets/applet_manifest.h"

#include <QString>

namespace QindaQt::AppletHost {

enum class PackageTrust {
    AuditedBuiltin,
    ThirdParty,
};

struct PackageIdentity final {
    QString packageId;
    PackageTrust trust = PackageTrust::ThirdParty;

    bool operator==(const PackageIdentity &) const = default;
};

enum class HostMode {
    InProcessAuditedBuiltin,
    SandboxRequiredProcess,
    Rejected,
};

struct HostSelection final {
    HostMode mode = HostMode::Rejected;
    QString reason;

    [[nodiscard]] bool accepted() const;
};

class HostSelector final {
public:
    // AGENT-CONTRACT: PackageTrust comes from an audited installation registry,
    // never from manifest content. A process launcher must refuse the sandbox-
    // required mode until a real confinement adapter confirms enforcement.
    [[nodiscard]] static HostSelection select(
        const QindaQt::Applets::AppletManifest &manifest,
        const PackageIdentity &package,
        const QindaQt::Applets::ApiVersion &hostApi = QindaQt::Applets::ApiVersion::current());
};

[[nodiscard]] QString toString(PackageTrust value);
[[nodiscard]] QString toString(HostMode value);

} // namespace QindaQt::AppletHost
