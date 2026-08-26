// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_runtime/applet_instance_resolver.h"

#include "qindaqt/applet_runtime/builtin_applet_registry.h"
#include "qindaqt/applets/manifest_catalog.h"
#include "qindaqt/applets/manifest_types.h"

#include <optional>
#include <utility>

namespace QindaQt::AppletRuntime {
namespace {

using Status = AppletResolutionStatus;

std::optional<Applets::Orientation> orientationFor(Profiles::Edge edge)
{
    switch (edge) {
    case Profiles::Edge::Top:
    case Profiles::Edge::Bottom:
        return Applets::Orientation::Horizontal;
    case Profiles::Edge::Left:
    case Profiles::Edge::Right:
        return Applets::Orientation::Vertical;
    }
    return std::nullopt;
}

std::optional<Applets::PlacementZone> placementFor(const QVariantMap &settings)
{
    const QString zone = settings.value(QStringLiteral("zone"),
                                        QStringLiteral("start")).toString();
    if (zone == QLatin1String("start")) {
        return Applets::PlacementZone::PanelStart;
    }
    if (zone == QLatin1String("center")) {
        return Applets::PlacementZone::PanelCenter;
    }
    if (zone == QLatin1String("end")) {
        return Applets::PlacementZone::PanelEnd;
    }
    if (zone == QLatin1String("fill")) {
        return Applets::PlacementZone::PanelFill;
    }
    return std::nullopt;
}

ResolvedAppletInstance failure(const Profiles::AppletSpec &instance,
                               Status status, QString diagnostic,
                               QString displayName = {}, QString entryPoint = {})
{
    return {instance, std::move(displayName), std::move(entryPoint), {},
            AppletHost::HostMode::Rejected, status, std::move(diagnostic)};
}

} // namespace

bool ResolvedAppletInstance::ready() const noexcept
{
    return status == AppletResolutionStatus::Ready;
}

QVariantMap ResolvedAppletInstance::toVariantMap() const
{
    QVariantMap result = instance.toVariantMap();
    result.insert(QStringLiteral("runtime"),
                  QVariantMap{{QStringLiteral("status"), toString(status)},
                              {QStringLiteral("ready"), ready()},
                              {QStringLiteral("displayName"), displayName},
                              {QStringLiteral("entryPoint"), entryPoint},
                              {QStringLiteral("hostMode"),
                               AppletHost::toString(hostMode)},
                              {QStringLiteral("grantedCapabilities"),
                               grantedCapabilities},
                              {QStringLiteral("diagnostic"), diagnostic}});
    return result;
}

ResolvedAppletInstance AppletInstanceResolver::resolveBuiltin(
    const Profiles::AppletSpec &instance,
    Profiles::Edge panelEdge,
    const Applets::ManifestCatalog &catalog,
    const AppletHost::CapabilityPolicy &policy,
    const BuiltinAppletRegistry &registry)
{
    const auto *manifest = catalog.findById(instance.plugin);
    if (manifest == nullptr) {
        return failure(instance, Status::MissingManifest,
                       QStringLiteral("no validated manifest exists for '%1'")
                           .arg(instance.plugin));
    }
    const auto placement = placementFor(instance.settings);
    const auto orientation = orientationFor(panelEdge);
    if (!placement || !manifest->placementZones.contains(*placement) ||
        !orientation || !manifest->orientations.contains(*orientation)) {
        return failure(instance, Status::PlacementRejected,
                       QStringLiteral("applet placement is not supported by its manifest"),
                       manifest->name, manifest->entryPoint.value);
    }

    const AppletHost::PackageIdentity package{
        manifest->id, AppletHost::PackageTrust::AuditedBuiltin};
    const auto host = AppletHost::HostSelector::select(*manifest, package);
    if (!host.accepted()) {
        return failure(instance, Status::HostRejected, host.reason, manifest->name,
                       manifest->entryPoint.value);
    }
    if (host.mode == AppletHost::HostMode::SandboxRequiredProcess) {
        return {instance, manifest->name, manifest->entryPoint.value, {}, host.mode,
                Status::SandboxUnavailable,
                QStringLiteral("sandbox process hosting is not available")};
    }
    if (!registry.contains(manifest->entryPoint.value)) {
        return {instance, manifest->name, manifest->entryPoint.value, {}, host.mode,
                Status::ImplementationUnavailable,
                QStringLiteral("builtin entry point is absent from the audited registry")};
    }

    const auto capabilities = policy.evaluate(*manifest, package);
    if (!capabilities.ok) {
        return {instance, manifest->name, manifest->entryPoint.value, {}, host.mode,
                Status::PolicyRejected, capabilities.error};
    }
    QStringList granted;
    for (const auto &decision : capabilities.decisions) {
        if (decision.granted()) {
            granted.append(Applets::toString(decision.capability));
        }
    }
    granted.sort();
    return {instance, manifest->name, manifest->entryPoint.value,
            std::move(granted), host.mode, Status::Ready, {}};
}

QString toString(AppletResolutionStatus value)
{
    switch (value) {
    case AppletResolutionStatus::Ready:
        return QStringLiteral("ready");
    case AppletResolutionStatus::MissingManifest:
        return QStringLiteral("missing-manifest");
    case AppletResolutionStatus::PlacementRejected:
        return QStringLiteral("placement-rejected");
    case AppletResolutionStatus::HostRejected:
        return QStringLiteral("host-rejected");
    case AppletResolutionStatus::SandboxUnavailable:
        return QStringLiteral("sandbox-unavailable");
    case AppletResolutionStatus::ImplementationUnavailable:
        return QStringLiteral("implementation-unavailable");
    case AppletResolutionStatus::PolicyRejected:
        return QStringLiteral("policy-rejected");
    }
    return {};
}

} // namespace QindaQt::AppletRuntime
