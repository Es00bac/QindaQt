// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "qindaqt/workspaces_ui/workspace_dialogs.h"

#include <QHash>
#include <QObject>

#include <functional>
#include <optional>

namespace QindaQt::WorkspacesApps {
class DesktopApplications;
}

namespace QindaQt::Compositor::KWinIntegration {

class HybridInteractionRuntime;
class ManagedWindowRegistry;

struct WorkspaceContainerPresentation final {
    QString name;
    QString color;
};

// Presentation remains owned by KWin session composition. The port only asks
// for current values and submits validated replacement values after adoption.
struct WorkspaceUiPortCallbacks final {
    std::function<std::optional<WorkspaceContainerPresentation>(const QString &)>
        readPresentation;
    // Must reject name/color values before a topology transaction begins.
    std::function<bool(const QString &, const QString &, QString *)>
        validatePresentation;
    std::function<bool(const QString &, const QString &, QString *)> renameContainer;
    std::function<bool(const QString &, const QString &, QString *)> setContainerColor;
    std::function<void()> invalidateScenePublication;
};

// Small owned facts make the eligibility rule testable without a KWin process.
struct WorkspaceWindowFacts final {
    bool exists = false;
    bool normal = false;
    bool independent = false;
    bool unowned = false;
};

[[nodiscard]] inline bool workspaceWindowIsEligible(
    const WorkspaceWindowFacts &facts) noexcept
{
    return facts.exists && facts.normal && facts.independent && facts.unowned;
}

[[nodiscard]] inline QString workspaceDesktopEntryId(QString desktopFileName,
                                                      QString resourceClass)
{
    return desktopFileName.trimmed().isEmpty()
        ? resourceClass.trimmed()
        : desktopFileName.trimmed();
}

// A presentation writer runs after the topology transaction. This text makes
// that committed state unambiguous to the dialog and its user.
[[nodiscard]] inline QString workspacePresentationWarning(QString detail = {})
{
    const auto prefix = QStringLiteral(
        "Workspace layout was restored, but its name and color could not be applied");
    return detail.isEmpty() ? prefix + QLatin1Char('.')
                            : prefix + QStringLiteral(": %1").arg(std::move(detail));
}

// GUI-thread bridge between QWidget dialogs and live KWin state. It borrows
// every collaborator, returns owned snapshots, and is invalid after any
// borrowed collaborator is destroyed. Every call and signal connection belongs
// on the GUI thread. It never reaches into KWinHybridSession.
class KWinWorkspaceUiPort final : public QObject,
                                  public WorkspacesUi::WorkspaceUiPort
{
    Q_OBJECT

public:
    KWinWorkspaceUiPort(ManagedWindowRegistry &registry,
                        HybridInteractionRuntime &runtime,
                        WorkspacesApps::DesktopApplications &applications,
                        WorkspaceUiPortCallbacks callbacks,
                        QObject *parent = nullptr);

    // Call before creating the library dialog: the dialog itself becomes KWin's
    // active window and is never an implicit choice of source container.
    [[nodiscard]] bool selectContainer(const QString &containerId,
                                       QString *error = nullptr);

    [[nodiscard]] std::optional<WorkspacesUi::CurrentContainer>
    currentContainer(QString *error) override;
    [[nodiscard]] QList<WorkspacesUi::WorkspaceWindow>
    availableWindows(QString *error) override;
    [[nodiscard]] std::optional<WorkspacesUi::InstalledApplication>
    installedApplication(const QString &desktopEntryId) const override;
    bool launchApplication(const QString &desktopEntryId,
                           const QStringList &urls,
                           QString *error) override;
    bool restore(const Workspaces::Workspace &workspace,
                 const Core::WindowContainer &boundLayout,
                 QString *error) override;

Q_SIGNALS:
    // Connect queued to WorkspaceLibraryDialog::reportLaunchFailure().
    void launchFailed(const QString &desktopEntryId, const QString &message);
    // The group was adopted, but a post-preflight presentation writer failed.
    // restore() still returns true so callers cannot retry the committed layout.
    void restoreWarning(const QString &message);

private:
    [[nodiscard]] bool hasSelectedContainer(QString *error) const;
    [[nodiscard]] QString desktopEntryIdForWindow(const QString &windowId) const;
    [[nodiscard]] QStringList layoutMembers(
        const Core::WindowContainer &container) const;
    [[nodiscard]] bool isLiveIndependentWindow(const QString &windowId) const;

    ManagedWindowRegistry &m_registry;
    HybridInteractionRuntime &m_runtime;
    WorkspacesApps::DesktopApplications &m_applications;
    WorkspaceUiPortCallbacks m_callbacks;
    QString m_selectedContainerId;
    QHash<QString, QString> m_workspaceIds;
};

} // namespace QindaQt::Compositor::KWinIntegration
