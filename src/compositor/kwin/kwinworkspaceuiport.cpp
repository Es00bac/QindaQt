// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinworkspaceuiport.h"

#include "hybridinteractionruntime.h"
#include "managedwindowregistry.h"
#include "qindaqt/workspaces_apps/desktop_applications.h"

#include <window.h>

#include <utility>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

bool fail(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
    return false;
}

void collectMembers(const Core::LayoutNode &node, QStringList &members)
{
    if (node.isLeaf()) {
        members.append(node.windowId());
        return;
    }
    collectMembers(*node.firstChild(), members);
    collectMembers(*node.secondChild(), members);
}

} // namespace

KWinWorkspaceUiPort::KWinWorkspaceUiPort(
    ManagedWindowRegistry &registry,
    HybridInteractionRuntime &runtime,
    WorkspacesApps::DesktopApplications &applications,
    WorkspaceUiPortCallbacks callbacks,
    QObject *parent)
    : QObject(parent)
    , m_registry(registry)
    , m_runtime(runtime)
    , m_applications(applications)
    , m_callbacks(std::move(callbacks))
{
    connect(&m_applications, &WorkspacesApps::DesktopApplications::launchFinished,
            this, [this](const QString &desktopEntryId, bool started,
                         const QString &message) {
                if (!started) {
                    Q_EMIT launchFailed(desktopEntryId, message);
                }
            });
}

bool KWinWorkspaceUiPort::selectContainer(const QString &containerId,
                                           QString *error)
{
    if (containerId.isEmpty()
        || !m_runtime.topology().container(containerId)) {
        m_selectedContainerId.clear();
        return fail(error, QStringLiteral("The selected container is unavailable."));
    }
    m_selectedContainerId = containerId;
    return true;
}

std::optional<WorkspacesUi::CurrentContainer>
KWinWorkspaceUiPort::currentContainer(QString *error)
{
    if (!hasSelectedContainer(error)) {
        return std::nullopt;
    }
    const auto *container = m_runtime.topology().container(m_selectedContainerId);
    const auto presentation = m_callbacks.readPresentation
        ? m_callbacks.readPresentation(m_selectedContainerId) : std::nullopt;
    if (!presentation) {
        fail(error, QStringLiteral("The selected container presentation is unavailable."));
        return std::nullopt;
    }
    QMap<QString, Workspaces::ApplicationSlot> applications;
    int slotNumber = 0;
    for (const QString &windowId : m_runtime.topology().windowIds(m_selectedContainerId)) {
        auto *const window = m_registry.window(windowId);
        if (!window || m_registry.owner(windowId) != m_selectedContainerId) {
            fail(error, QStringLiteral("A container window is no longer available."));
            return std::nullopt;
        }
        const auto desktopEntryId = desktopEntryIdForWindow(windowId);
        if (desktopEntryId.isEmpty()) {
            fail(error, QStringLiteral("A container window has no application identity."));
            return std::nullopt;
        }
        const auto title = window->caption().trimmed();
        applications.insert(windowId,
                            {QStringLiteral("slot-%1").arg(++slotNumber),
                             title.isEmpty() ? desktopEntryId : title,
                             desktopEntryId,
                             {}});
    }
    return WorkspacesUi::CurrentContainer{
        *container,
        applications,
        presentation->name,
        presentation->color,
        m_workspaceIds.value(m_selectedContainerId),
    };
}

QList<WorkspacesUi::WorkspaceWindow>
KWinWorkspaceUiPort::availableWindows(QString *error)
{
    if (!m_runtime.ready()) {
        fail(error, m_runtime.initializationError());
        return {};
    }
    QList<WorkspacesUi::WorkspaceWindow> result;
    for (const QString &windowId : m_registry.windowIds()) {
        auto *const window = m_registry.window(windowId);
        const WorkspaceWindowFacts facts{
            window != nullptr,
            window && window->isNormalWindow() && !window->isDeleted(),
            m_runtime.topology().isIndependent(windowId),
            m_registry.owner(windowId).isEmpty(),
        };
        if (!workspaceWindowIsEligible(facts)) {
            continue;
        }
        result.append({{windowId, desktopEntryIdForWindow(windowId), true},
                       window->caption()});
    }
    return result;
}

std::optional<WorkspacesUi::InstalledApplication>
KWinWorkspaceUiPort::installedApplication(const QString &desktopEntryId) const
{
    const auto application = m_applications.find(desktopEntryId);
    if (!application) {
        return std::nullopt;
    }
    return WorkspacesUi::InstalledApplication{application->id, application->name};
}

bool KWinWorkspaceUiPort::launchApplication(const QString &desktopEntryId,
                                             const QStringList &urls,
                                             QString *error)
{
    return m_applications.launch(desktopEntryId, urls, {}, error);
}

bool KWinWorkspaceUiPort::restore(const Workspaces::Workspace &workspace,
                                  const Core::WindowContainer &boundLayout,
                                  QString *error)
{
    if (!workspace.validate(error)) {
        return false;
    }
    const auto layoutValidation = boundLayout.validate();
    if (!layoutValidation.valid) {
        return fail(error, QStringLiteral("The assigned workspace layout is invalid: %1")
                               .arg(layoutValidation.message));
    }
    if (!m_runtime.ready()) {
        return fail(error, m_runtime.initializationError());
    }
    QString presentationError;
    if (!m_callbacks.validatePresentation
        || !m_callbacks.validatePresentation(workspace.name, workspace.color,
                                             &presentationError)) {
        return fail(error, presentationError.isEmpty()
                               ? QStringLiteral("The workspace name or color is invalid.")
                               : presentationError);
    }
    for (const QString &windowId : layoutMembers(boundLayout)) {
        if (!isLiveIndependentWindow(windowId)) {
            return fail(error, QStringLiteral(
                "A chosen window is no longer available. Refresh the window list."));
        }
    }
    // AGENT-GUARD: One runtime call owns scene preparation and topology publish.
    // Do not pre-assign registry ownership or retry after a failed adoption.
    const auto adoption = m_runtime.adoptIndependentLayout(boundLayout);
    if (!adoption.topologyChanged()) {
        return fail(error, adoption.message.isEmpty()
                               ? QStringLiteral("The workspace layout was not restored.")
                               : adoption.message);
    }
    // The layout now exists even when the later presentation projection fails.
    // Preserve its template identity so a later Save updates the same document.
    m_workspaceIds.insert(boundLayout.id(), workspace.id);
    m_selectedContainerId = boundLayout.id();
    if (!m_callbacks.renameContainer || !m_callbacks.setContainerColor
        || !m_callbacks.renameContainer(boundLayout.id(), workspace.name,
                                        &presentationError)
        || !m_callbacks.setContainerColor(boundLayout.id(), workspace.color,
                                          &presentationError)) {
        const auto warning = workspacePresentationWarning(presentationError);
        if (m_callbacks.invalidateScenePublication) {
            m_callbacks.invalidateScenePublication();
        }
        Q_EMIT restoreWarning(warning);
        // Adoption committed. A false result would invite the dialog to retry
        // windows which the runtime now owns; delivery of the warning is the
        // separate presentation-result channel.
        return true;
    }
    if (m_callbacks.invalidateScenePublication) {
        m_callbacks.invalidateScenePublication();
    }
    return true;
}

bool KWinWorkspaceUiPort::hasSelectedContainer(QString *error) const
{
    if (!m_runtime.ready()) {
        return fail(error, m_runtime.initializationError());
    }
    if (m_selectedContainerId.isEmpty()
        || !m_runtime.topology().container(m_selectedContainerId)) {
        return fail(error, QStringLiteral("Choose a container before saving."));
    }
    return true;
}

QString KWinWorkspaceUiPort::desktopEntryIdForWindow(const QString &windowId) const
{
    const auto *const window = m_registry.window(windowId);
    return window ? workspaceDesktopEntryId(window->desktopFileName(),
                                            window->resourceClass())
                  : QString{};
}

QStringList KWinWorkspaceUiPort::layoutMembers(
    const Core::WindowContainer &container) const
{
    QStringList members;
    for (const auto &page : container.pages()) {
        collectMembers(page.root(), members);
    }
    return members;
}

bool KWinWorkspaceUiPort::isLiveIndependentWindow(const QString &windowId) const
{
    const auto *const window = m_registry.window(windowId);
    return workspaceWindowIsEligible({
        window != nullptr,
        window && window->isNormalWindow() && !window->isDeleted(),
        m_runtime.topology().isIndependent(windowId),
        m_registry.owner(windowId).isEmpty(),
    });
}

} // namespace QindaQt::Compositor::KWinIntegration
