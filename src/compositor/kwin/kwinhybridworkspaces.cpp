// SPDX-License-Identifier: GPL-3.0-or-later
#include "kwinhybridsession.h"
#include "hybridinteractionruntime.h"
#include "kwinworkspacecontroller.h"
#include "kwinworkspaceuiport.h"
#include "managedwindowregistry.h"
#include "qindaqt/compositor/containerappearance.h"
#include "qindaqt/workspaces_apps/desktop_applications.h"

#include <workspace.h>
#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Compositor::KWinIntegration {
void KWinHybridSession::initializeSavedWorkspaces()
{
    m_workspaceApplications = std::make_unique<WorkspacesApps::DesktopApplications>();
    WorkspaceUiPortCallbacks callbacks;
    callbacks.readPresentation = [this](const QString &id)
        -> std::optional<WorkspaceContainerPresentation> {
        if (m_shutdown || !m_runtime->topology().container(id)) {
            return std::nullopt;
        }
        const auto appearance = containerAppearance(id);
        return WorkspaceContainerPresentation{appearance.name, appearance.colorHex};
    };
    callbacks.validatePresentation = [](const QString &name, const QString &color,
                                         QString *error) {
        const bool validName = !normalizedContainerName(name).isEmpty();
        const bool validColor = color.isEmpty()
            || !normalizedContainerColor(color).isEmpty();
        if (error) {
            *error = validName && validColor ? QString{}
                : QStringLiteral("The workspace name or color cannot be used.");
        }
        return validName && validColor;
    };
    callbacks.renameContainer = [this](const QString &id, const QString &name,
                                       QString *error) {
        return renameContainer(id, name, error);
    };
    callbacks.setContainerColor = [this](const QString &id, const QString &color,
                                        QString *error) {
        return setContainerColor(id, color, error);
    };
    callbacks.invalidateScenePublication = [this] {
        synchronizeChrome();
        Q_EMIT shellVisibilityStateChanged();
    };
    m_workspacePort = std::make_unique<KWinWorkspaceUiPort>(
        m_registry, *m_runtime, *m_workspaceApplications, std::move(callbacks));
    // AGENT-CONTRACT: This explicit directory is shared across compositor
    // lifetimes. AppDataLocation would silently key saved workspaces to KWin's
    // executable/application name instead of QindaQt's durable product identity.
    const auto storage = QDir(QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation)).filePath(QStringLiteral("qindaqt/workspaces"));
    m_workspaceController = std::make_unique<KWinWorkspaceController>(
        *m_workspacePort, [this] {
            if (m_shutdown || !m_runtime) {
                return QString{};
            }
            const auto id = m_registry.windowId(KWin::workspace()->activeWindow());
            return m_runtime->topology().ownerOf(id).value_or(QString{});
        }, storage);
    m_workspaceController->setPalette(m_nativePalette);
}

void KWinHybridSession::showSavedWorkspaces(const QString &containerId)
{
    if (!m_shutdown && m_workspaceController) {
        m_workspaceController->showLibraryForContainer(containerId);
    }
}

void KWinHybridSession::shutdownSavedWorkspaces() noexcept
{
    m_workspaceController.reset();
    m_workspacePort.reset();
    m_workspaceApplications.reset();
}
} // namespace QindaQt::Compositor::KWinIntegration
