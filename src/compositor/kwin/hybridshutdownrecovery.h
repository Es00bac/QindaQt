// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <QStringList>

#include <functional>

namespace QindaQt::Compositor::KWinIntegration {

struct HybridShutdownSnapshot final
{
    QStringList topologyWindowIds;
    QStringList liveOwnedWindowIds;
    qsizetype containerCount = 0;
};

struct HybridShutdownRecoveryCallbacks final
{
    std::function<HybridShutdownSnapshot()> snapshot;
    std::function<bool(const QString &windowId)> windowExists;
    std::function<bool(const QString &windowId, QString *error)> forgetClosedWindow;
    std::function<bool(QString *error)> releaseAll;
    std::function<bool(QString *error)> fallbackCleanup;
};

struct HybridShutdownRecoveryResult final
{
    bool complete = false;
    bool fallbackUsed = false;
    int releaseAttempts = 0;
    int reconciledClosedWindows = 0;
    QStringList diagnostics;
};

// Coordinates bounded unload recovery without owning topology or KWin state.
// All callbacks are synchronous and run on the compositor thread. The caller
// must keep every callback target alive until recover() returns.
class HybridShutdownRecovery final
{
public:
    [[nodiscard]] static HybridShutdownRecoveryResult recover(
        const HybridShutdownRecoveryCallbacks &callbacks,
        int maximumReleaseAttempts = 3);
};

} // namespace QindaQt::Compositor::KWinIntegration
