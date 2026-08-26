// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_host/host_selection.h"

#include "qindaqt/applets/manifest_types.h"

namespace QindaQt::AppletHost {

bool HostSelection::accepted() const
{
    return mode != HostMode::Rejected;
}

HostSelection HostSelector::select(const QindaQt::Applets::AppletManifest &manifest,
                                   const PackageIdentity &package,
                                   const QindaQt::Applets::ApiVersion &hostApi)
{
    const QindaQt::Applets::ManifestValidation validation = manifest.validate();
    if (!validation.isValid()) {
        return {HostMode::Rejected,
                QStringLiteral("Invalid applet manifest: %1").arg(validation.summary())};
    }
    if (package.packageId != manifest.id) {
        return {HostMode::Rejected,
                QStringLiteral("Package identity does not match manifest id")};
    }
    if (!manifest.supportsHost(hostApi)) {
        return {HostMode::Rejected,
                QStringLiteral("Applet API %1 is not supported by host API %2")
                    .arg(manifest.apiVersion.toString(), hostApi.toString())};
    }

    if (manifest.entryPoint.kind == QindaQt::Applets::EntryPointKind::Builtin) {
        if (package.trust != PackageTrust::AuditedBuiltin) {
            return {HostMode::Rejected,
                    QStringLiteral("Third-party packages cannot resolve compiled-in entry points")};
        }
        return {HostMode::InProcessAuditedBuiltin,
                QStringLiteral("Audited registry entry may use its compiled-in implementation")};
    }

    return {HostMode::SandboxRequiredProcess,
            QStringLiteral("Dynamic applet code requires an out-of-process sandbox")};
}

QString toString(PackageTrust value)
{
    switch (value) {
    case PackageTrust::AuditedBuiltin:
        return QStringLiteral("audited-builtin");
    case PackageTrust::ThirdParty:
        return QStringLiteral("third-party");
    }
    return {};
}

QString toString(HostMode value)
{
    switch (value) {
    case HostMode::InProcessAuditedBuiltin:
        return QStringLiteral("in-process-audited-builtin");
    case HostMode::SandboxRequiredProcess:
        return QStringLiteral("sandbox-required-process");
    case HostMode::Rejected:
        return QStringLiteral("rejected");
    }
    return {};
}

} // namespace QindaQt::AppletHost
