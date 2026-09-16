// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"

#include "hybridinteractionruntime.h"
#include "managedwindowregistry.h"
#include "kwinworkspaceuiport.h"
#include "qindaqt/workspaces_apps/desktop_applications.h"

#include <core/output.h>
#include <window.h>
#include <workspace.h>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

using WorkspacesApps::DesktopApplications;

// A picker may sit unchosen for a whole working session; once its user picks
// an application the wait is bounded to that launch's window arrival.
constexpr int pickerIdleLifetimeMinutes = 10;
constexpr int pickerLaunchArrivalSeconds = 90;

QByteArray chooserResponse(QString status, QString message = {})
{
    QJsonObject object{{QStringLiteral("status"), std::move(status)}};
    if (!message.isEmpty()) {
        object.insert(QStringLiteral("message"), std::move(message));
    }
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

} // namespace

// Window-facts access helper: the registry owns KWin::Window lookup, so this
// translation-unit-local accessor keeps the lifecycle hooks declarative.
void KWinHybridSession::purgeExpiredPickerReplacements()
{
    const auto now = QDateTime::currentDateTime();
    QHash<QString, PendingPickerReplacement>::iterator it =
        m_pendingPickerReplacements.begin();
    while (it != m_pendingPickerReplacements.end()) {
        if (it->expiresAt <= now) {
            it = m_pendingPickerReplacements.erase(it);
        } else {
            ++it;
        }
    }
}

bool KWinHybridSession::registerPickerReplacement(const QString &containerId,
                                                  const QString &pickerWindowId,
                                                  const QString &desktopEntryId,
                                                  QString *error)
{
    if (!ready() || !m_runtime->topology().container(containerId)) {
        return fail(error, QStringLiteral("The restored container is unavailable."));
    }
    if (!m_runtime->topology().windowIds(containerId).contains(pickerWindowId)) {
        return fail(error,
                    QStringLiteral("The picker window is not a member of the container."));
    }
    if (desktopEntryId.isEmpty()) {
        return fail(error, QStringLiteral("The slot has no application identity."));
    }
    // AGENT-GUARD: Re-registering a picker must not orphan the previous
    // pending entry; one picker window holds at most one pending replacement.
    m_pendingPickerReplacements.insert(
        pickerWindowId,
        PendingPickerReplacement{containerId, desktopEntryId,
                                 QDateTime::currentDateTime().addSecs(
                                     pickerIdleLifetimeMinutes * 60),
                                 false});
    return true;
}

QByteArray KWinHybridSession::handleWorkspaceChooserRequest(
    const QString &desktopEntryId, qint64 callerProcessId)
{
    purgeExpiredPickerReplacements();
    auto *const activeWindow =
        KWin::workspace() ? KWin::workspace()->activeWindow() : nullptr;
    const QString activeWindowId = m_registry.windowId(activeWindow);
    auto pendingIt = m_pendingPickerReplacements.find(activeWindowId);
    if (pendingIt == m_pendingPickerReplacements.end()) {
        // ADR-0172: a window may replace ITSELF. An application browsing the
        // installed applications while docked in a container asks for the
        // chosen application to take its place, which is the same atomic
        // ReplaceMemberWindow transaction a restored picker uses.
        //
        // AGENT-GUARD: the active window must belong to THIS caller, compared
        // against KWin's authenticated client PID. Without that check any
        // client on the bus could replace whatever window happened to be
        // focused - someone else's application - by calling this route.
        if (activeWindow == nullptr || callerProcessId <= 0
            || static_cast<qint64>(activeWindow->pid()) != callerProcessId) {
            return chooserResponse(
                QStringLiteral("rejected"),
                QStringLiteral("The active window is not a workspace picker."));
        }
        const QString containerId = m_registry.owner(activeWindowId);
        if (containerId.isEmpty()) {
            return chooserResponse(
                QStringLiteral("rejected"),
                QStringLiteral("This window is not docked in a container."));
        }
        QString registrationError;
        if (!registerPickerReplacement(containerId, activeWindowId,
                                       desktopEntryId, &registrationError)) {
            return chooserResponse(QStringLiteral("rejected"), registrationError);
        }
        pendingIt = m_pendingPickerReplacements.find(activeWindowId);
        if (pendingIt == m_pendingPickerReplacements.end()) {
            return chooserResponse(
                QStringLiteral("rejected"),
                QStringLiteral("The replacement could not be registered."));
        }
    }
    if (pendingIt->launched) {
        return chooserResponse(QStringLiteral("rejected"),
                               QStringLiteral("A replacement application is already starting."));
    }
    if (!m_workspaceApplications) {
        return chooserResponse(QStringLiteral("rejected"),
                               QStringLiteral("Application launching is unavailable."));
    }
    const auto application = m_workspaceApplications->find(desktopEntryId);
    if (!application) {
        return chooserResponse(QStringLiteral("rejected"),
                               QStringLiteral("Application %1 is not installed.")
                                   .arg(desktopEntryId));
    }
    QString error;
    // AGENT-NOTE: dispatch-only truth (ADR-0101). The picker stays open until
    // the arriving window is actually swapped in, so a failed startup leaves
    // the slot placeholder usable instead of silently vanished.
    if (!m_workspaceApplications->launch(desktopEntryId, {}, {}, &error)) {
        return chooserResponse(QStringLiteral("rejected"),
                               error.isEmpty()
                                   ? QStringLiteral("The application could not be launched.")
                                   : error);
    }
    pendingIt->launched = true;
    pendingIt->desktopEntryId = desktopEntryId;
    pendingIt->expiresAt = QDateTime::currentDateTime().addSecs(
        pickerLaunchArrivalSeconds);
    return chooserResponse(QStringLiteral("ok"),
                           QStringLiteral("Starting %1; it will replace this picker.")
                               .arg(application->name));
}

void KWinHybridSession::correlateArrivingWindow(const QString &windowId)
{
    purgeExpiredPickerReplacements();
    if (m_pendingPickerReplacements.isEmpty()) {
        return;
    }
    auto *const window = m_registry.window(windowId);
    if (!window || !window->isNormalWindow() || window->isDeleted()
        || !m_runtime->topology().isIndependent(windowId)
        || !m_registry.owner(windowId).isEmpty()) {
        return;
    }
    // ADR-0165: identity matching reuses the workspaces' established
    // desktopFileName-then-resourceClass rule; the first eligible pending
    // entry wins and the arrival is consumed by it.
    const auto arrivingEntryId = workspaceDesktopEntryId(window->desktopFileName(),
                                                         window->resourceClass());
    for (auto it = m_pendingPickerReplacements.begin();
         it != m_pendingPickerReplacements.end(); ++it) {
        if (!it->launched || it->desktopEntryId != arrivingEntryId) {
            continue;
        }
        QString focusError;
        if (!restoreMemberFocusForLifecycleChange(&focusError)) {
            qWarning("QindaQt picker replacement could not leave member focus: %s",
                     qPrintable(focusError));
            return;
        }
        const auto result = m_runtime->replaceMemberWindow(
            it->containerId, it.key(), windowId);
        if (!result.topologyChanged()) {
            qWarning("QindaQt picker replacement failed: %s",
                     qPrintable(result.message));
            return;
        }
        // AGENT-NOTE: the picker window was swapped out to the independent
        // set; closing it now is the user-visible "the app took my place".
        if (auto *const picker = m_registry.window(it.key())) {
            picker->closeWindow();
        }
        m_pendingPickerReplacements.erase(it);
        synchronizeChrome();
        return;
    }
}

void KWinHybridSession::forgetPickerReplacement(
    const QString &pickerWindowId) noexcept
{
    m_pendingPickerReplacements.remove(pickerWindowId);
}

} // namespace QindaQt::Compositor::KWinIntegration
