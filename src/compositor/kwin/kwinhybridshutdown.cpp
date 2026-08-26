// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridshutdown.h"

#include "hybridinteractionruntime.h"
#include "kwinhybridscene.h"
#include "managedwindowregistry.h"

namespace QindaQt::Compositor::KWinIntegration {
namespace {

HybridShutdownSnapshot snapshot(HybridInteractionRuntime &runtime,
                                ManagedWindowRegistry &registry)
{
    HybridShutdownSnapshot result;
    const auto &topology = runtime.topology();
    result.containerCount = topology.containerIds().size();
    for (const auto &containerId : topology.containerIds()) {
        for (const auto &windowId : topology.windowIds(containerId)) {
            result.topologyWindowIds.append(windowId);
            if (registry.window(windowId)
                && registry.owner(windowId) == containerId) {
                result.liveOwnedWindowIds.append(windowId);
            }
        }
    }
    result.topologyWindowIds.removeDuplicates();
    result.liveOwnedWindowIds.removeDuplicates();
    result.topologyWindowIds.sort();
    result.liveOwnedWindowIds.sort();
    return result;
}

} // namespace

HybridShutdownRecoveryResult recoverKWinHybridShutdown(
    HybridInteractionRuntime &runtime,
    KWinHybridSceneFactory &sceneFactory,
    ManagedWindowRegistry &registry)
{
    return HybridShutdownRecovery::recover({
        .snapshot = [&] { return snapshot(runtime, registry); },
        .windowExists = [&](const QString &windowId) {
            return registry.window(windowId) != nullptr;
        },
        .forgetClosedWindow = [&](const QString &windowId, QString *error) {
            const auto forgotten = runtime.forgetWindow(windowId);
            if (forgotten.topologyChanged()
                || forgotten.status == HybridRuntimeStatus::NoChange) {
                return true;
            }
            if (error) {
                *error = forgotten.message;
            }
            return false;
        },
        .releaseAll = [&](QString *error) {
            const auto released = runtime.releaseAll();
            if (!released.complete && error) {
                *error = released.message;
            }
            return released.complete;
        },
        .fallbackCleanup = [&](QString *error) {
            const auto cleaned = sceneFactory.emergencyReleaseAll(runtime.topology());
            if (!cleaned.succeeded && error) {
                *error = cleaned.message;
            }
            return cleaned.succeeded;
        },
    });
}

} // namespace QindaQt::Compositor::KWinIntegration
