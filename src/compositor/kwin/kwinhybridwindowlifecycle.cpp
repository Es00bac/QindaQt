// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridcontainerplacement.h"
#include "hybridinteractionruntime.h"

namespace QindaQt::Compositor::KWinIntegration {
namespace {

void warnLifecycleFailure(QLatin1StringView operation,
                          const HybridRuntimeResult &result)
{
    if (result.status == HybridRuntimeStatus::Rejected
        || result.status == HybridRuntimeStatus::Unsupported
        || result.status == HybridRuntimeStatus::NeedsGeometry) {
        qWarning("QindaQt Hybrid %s failed: %s",
                 qPrintable(QString(operation)), qPrintable(result.message));
    }
}

} // namespace

void KWinHybridSession::addManagedWindow(const QString &windowId)
{
    if (!ready()) {
        return;
    }
    QString error;
    if (!restoreMemberFocusForLifecycleChange(&error)) {
        qWarning("QindaQt window registration could not leave member focus: %s",
                 qPrintable(error));
        return;
    }
    const auto result = m_runtime->addWindow(windowId);
    warnLifecycleFailure(QLatin1StringView("window registration"), result);
    if (result.topologyChanged()) {
        synchronizeChrome();
    }
}

void KWinHybridSession::forgetManagedWindow(const QString &windowId)
{
    if (!ready()) {
        return;
    }
    QString error;
    if (!restoreMemberFocusForLifecycleChange(&error)) {
        qWarning("QindaQt window removal could not leave member focus: %s",
                 qPrintable(error));
        return;
    }

    // AGENT-GUARD: Add/Forget scene transactions re-plan every group. Member
    // focus must be gone first, while PreserveCurrent activation retains the
    // app KWin selected before emitting the lifecycle signal.
    const auto beforeContainers = m_runtime->topology().containerIds();
    const auto result = m_runtime->forgetWindow(windowId);
    warnLifecycleFailure(QLatin1StringView("window removal"), result);
    if (!result.topologyChanged()) {
        return;
    }
    const auto afterContainers = m_runtime->topology().containerIds();
    for (const auto &id : beforeContainers) {
        if (!afterContainers.contains(id)) {
            m_placement->forgetContainer(id);
            m_minimizedContainers.remove(id);
        }
    }
    synchronizeChrome();
}

} // namespace QindaQt::Compositor::KWinIntegration
