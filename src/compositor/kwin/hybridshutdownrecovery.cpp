// SPDX-License-Identifier: GPL-3.0-or-later
#include "hybridshutdownrecovery.h"

#include <algorithm>
#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool externallyClean(const HybridShutdownSnapshot &snapshot)
{
    return snapshot.containerCount == 0 && snapshot.liveOwnedWindowIds.isEmpty();
}

void appendDiagnostic(HybridShutdownRecoveryResult &result,
                      QString operation,
                      const QString &detail)
{
    result.diagnostics.append(detail.isEmpty()
                                  ? std::move(operation)
                                  : QStringLiteral("%1: %2").arg(operation, detail));
}

} // namespace

HybridShutdownRecoveryResult HybridShutdownRecovery::recover(
    const HybridShutdownRecoveryCallbacks &callbacks,
    int maximumReleaseAttempts)
{
    HybridShutdownRecoveryResult result;
    if (!callbacks.snapshot || !callbacks.windowExists
        || !callbacks.forgetClosedWindow || !callbacks.releaseAll
        || !callbacks.fallbackCleanup) {
        result.diagnostics.append(
            QStringLiteral("shutdown recovery callbacks are incomplete"));
        return result;
    }

    maximumReleaseAttempts = std::max(1, maximumReleaseAttempts);
    auto snapshot = callbacks.snapshot();
    if (externallyClean(snapshot)) {
        result.complete = true;
        return result;
    }

    for (int attempt = 0; attempt < maximumReleaseAttempts; ++attempt) {
        // AGENT-GUARD: The normal lifecycle observer is disconnected before
        // unload recovery. Explicitly forget clients that raced with teardown
        // or every retry will reproduce the same stale topology transaction.
        for (const auto &windowId : snapshot.topologyWindowIds) {
            if (callbacks.windowExists(windowId)) {
                continue;
            }
            QString error;
            if (callbacks.forgetClosedWindow(windowId, &error)) {
                ++result.reconciledClosedWindows;
            } else {
                appendDiagnostic(result,
                                 QStringLiteral("could not reconcile closed window '%1'")
                                     .arg(windowId),
                                 error);
            }
        }

        QString error;
        ++result.releaseAttempts;
        if (!callbacks.releaseAll(&error)) {
            appendDiagnostic(result,
                             QStringLiteral("release attempt %1 failed")
                                 .arg(result.releaseAttempts),
                             error);
        }
        snapshot = callbacks.snapshot();
        if (externallyClean(snapshot)) {
            result.complete = true;
            return result;
        }
    }

    // A bounded fallback is necessary because plugin destruction removes the
    // only process-local restore authority. The fallback must directly clear
    // live ownership and restore state; it need not publish another topology
    // because that repository is destroyed immediately after this call.
    result.fallbackUsed = true;
    QString error;
    const bool fallbackSucceeded = callbacks.fallbackCleanup(&error);
    if (!fallbackSucceeded) {
        appendDiagnostic(result, QStringLiteral("fallback cleanup failed"), error);
    }
    snapshot = callbacks.snapshot();
    result.complete = fallbackSucceeded && snapshot.liveOwnedWindowIds.isEmpty();
    if (!result.complete && fallbackSucceeded) {
        result.diagnostics.append(
            QStringLiteral("fallback returned success while live owners remained"));
    }
    return result;
}

} // namespace QindaQt::Compositor::KWinIntegration
