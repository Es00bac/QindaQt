// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinrescueoutputs.h"

#include <core/output.h>
#include <virtualdesktops.h>
#include <workspace.h>

namespace QindaQt::Compositor::KWinIntegration {

QVector<RescueOutput> currentRescueOutputs()
{
    QVector<RescueOutput> result;
    auto *const workspace = KWin::workspace();
    if (!workspace) {
        return result;
    }
    auto *const desktops = KWin::VirtualDesktopManager::self();
    const KWin::VirtualDesktop *desktop = desktops ? desktops->currentDesktop() : nullptr;
    for (const KWin::LogicalOutput *output : workspace->outputs()) {
        if (!output) {
            continue;
        }
        const QRect geometry = output->geometry();
        const QRect workArea = desktop
            ? QRectF(workspace->clientArea(KWin::MaximizeArea, output, desktop)).toAlignedRect()
            : geometry;
        result.append({geometry, workArea});
    }
    return result;
}

QRectF onLiveOutput(const QRectF &savedFrame)
{
    return rescuedFrame(savedFrame, currentRescueOutputs()).value_or(savedFrame);
}

} // namespace QindaQt::Compositor::KWinIntegration
