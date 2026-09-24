// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktopmenutargets.h"

#include "launcher_applet_controller.h"

#include "qindaqt/shell/clipboard_applet/clipboard_applet_controller.h"
#include "qindaqt/shell/desktop_controls/places_controller.h"
#include "qindaqt/shell/desktop_controls/system_menu_controller.h"
#include "qindaqt/shell/desktop_menu/desktop_menu_model.h"
#include "qindaqt/shell/workspaces/workspace_controller.h"

#include <QMetaMethod>
#include <QVariantMap>

#include <utility>

namespace QindaQt::Shell {
namespace {

using DesktopMenu::Capability;
using DesktopMenu::DesktopCommand;
using SurfaceCommand = DesktopMenu::DesktopSurfaceCommands::Command;

[[nodiscard]] Capability capability(bool present, bool enabled)
{
    return {present, present && enabled};
}

} // namespace

ShellDesktopMenuTargets::ShellDesktopMenuTargets(Controllers controllers, Hooks hooks,
                                                 QObject *parent)
    : DesktopMenuTargets(parent)
    , m_controllers(controllers)
    , m_hooks(std::move(hooks))
{
    if (m_controllers.systemMenu != nullptr) {
        connect(m_controllers.systemMenu, &DesktopControls::SystemMenuController::stateChanged,
                this, &ShellDesktopMenuTargets::notifyFactsChanged);
    }
    if (QObject *session = sessionActions()) {
        // The session facade is lent as a QObject (as to QML), so its change
        // signals are joined by signature; a facade without them stays static.
        const QMetaMethod notify =
            staticMetaObject.method(staticMetaObject.indexOfSlot("notifyFactsChanged()"));
        for (const char *signature : {"availabilityChanged()", "pendingChanged()"}) {
            const int index = session->metaObject()->indexOfSignal(signature);
            if (index >= 0) {
                connect(session, session->metaObject()->method(index), this, notify);
            }
        }
    }
    if (m_controllers.launcher != nullptr) {
        connect(m_controllers.launcher, &Launcher::LauncherAppletController::stateChanged, this,
                &ShellDesktopMenuTargets::notifyFactsChanged);
    }
    if (m_controllers.workspaces != nullptr) {
        connect(m_controllers.workspaces, &Workspaces::WorkspaceController::stateChanged, this,
                &ShellDesktopMenuTargets::notifyFactsChanged);
    }
    if (m_controllers.desktopSurface != nullptr) {
        connect(m_controllers.desktopSurface, &DesktopMenu::DesktopSurfaceCommands::stateChanged,
                this, &ShellDesktopMenuTargets::notifyFactsChanged);
    }
}

QString ShellDesktopMenuTargets::helpDesktopEntryId()
{
    return QStringLiteral("org.qindaqt.Welcome");
}

void ShellDesktopMenuTargets::setHostedApplets(HostedApplets hosted)
{
    if (m_hosted == hosted) {
        return;
    }
    m_hosted = hosted;
    notifyFactsChanged();
}

void ShellDesktopMenuTargets::notifyFactsChanged()
{
    Q_EMIT factsChanged();
}

QObject *ShellDesktopMenuTargets::sessionActions() const
{
    return m_controllers.systemMenu != nullptr ? m_controllers.systemMenu->sessionActions()
                                               : nullptr;
}

DesktopMenu::DesktopMenuFacts ShellDesktopMenuTargets::facts() const
{
    DesktopMenu::DesktopMenuFacts facts;

    const bool routes = static_cast<bool>(m_hooks.openSettingsRoute);
    facts.aboutComputer = capability(routes, true);
    facts.keyboardShortcuts = capability(routes, true);

    const auto *systemMenu = m_controllers.systemMenu;
    facts.systemSettings =
        capability(systemMenu != nullptr, systemMenu != nullptr && systemMenu->canOpenSettings());
    // Mirrors the system menu applet's sessionEnabled(): the facade's own
    // admission, and nothing while one request is still pending.
    const QObject *session = sessionActions();
    const bool pending = session != nullptr && session->property("pending").toBool();
    const auto sessionCapability = [session, pending](const char *property) {
        return capability(session != nullptr,
                          session != nullptr && session->property(property).toBool() && !pending);
    };
    facts.lockScreen = sessionCapability("canLock");
    facts.logOut = sessionCapability("canLogout");
    facts.suspend = sessionCapability("canSuspend");
    facts.restart = sessionCapability("canReboot");
    facts.shutDown = sessionCapability("canPowerOff");

    const auto *launcher = m_controllers.launcher;
    const bool launchable = launcher != nullptr && launcher->launchGranted();
    facts.newFileManagerWindow = capability(launcher != nullptr, launchable);
    facts.help = capability(launcher != nullptr, launchable);
    facts.find = capability(launcher != nullptr && m_hosted.launcher, true);

    const auto *surface = m_controllers.desktopSurface;
    const bool attached = surface != nullptr && surface->surfaceAttached();
    facts.newFolder = capability(attached, true);
    facts.selectAll = capability(attached, true);
    facts.cleanUp = capability(attached, true);
    facts.paste = capability(attached, attached && surface->pasteAvailable());

    const auto *clipboard = m_controllers.clipboard;
    facts.clipboardHistory = capability(clipboard != nullptr && m_hosted.clipboard,
                                        clipboard != nullptr && clipboard->clipboardReadGranted());

    const auto *workspaces = m_controllers.workspaces;
    facts.showDesktop =
        capability(workspaces != nullptr, workspaces != nullptr && workspaces->canShowDesktop());
    facts.showingDesktop = workspaces != nullptr && workspaces->showingDesktop();
    if (workspaces != nullptr) {
        facts.workspaces = capability(workspaces->count() > 0, workspaces->canSwitch());
        facts.workspaceRevision = workspaces->revision();
        for (const QVariant &value : workspaces->rows()) {
            const QVariantMap row = value.toMap();
            facts.workspaceList.append({row.value(QStringLiteral("id")).toString(),
                                        row.value(QStringLiteral("name")).toString(),
                                        row.value(QStringLiteral("current")).toBool()});
        }
    }

    facts.gatherOverview = capability(static_cast<bool>(m_hooks.toggleGatherOverview), true);

    const auto *places = m_controllers.places;
    if (places != nullptr) {
        facts.places = capability(places->count() > 0, places->available());
        for (const DesktopControls::PlaceEntry &entry : places->entries()) {
            facts.placeList.append({entry.id, entry.label});
        }
    }

    const bool note = m_hooks.toggleShortcutNote && m_hooks.shortcutNoteVisible;
    facts.shortcutNote = capability(note, true);
    facts.shortcutNoteVisible = note && m_hooks.shortcutNoteVisible();
    return facts;
}

bool ShellDesktopMenuTargets::perform(const DesktopMenu::DesktopMenuCommand &command)
{
    switch (command.kind) {
    case DesktopCommand::AboutComputer:
        return openRoute(QStringLiteral("about-computer"), {});
    case DesktopCommand::KeyboardShortcuts:
        return openRoute(QStringLiteral("input"), QStringLiteral("shortcuts"));
    case DesktopCommand::SystemSettings:
        if (m_controllers.systemMenu == nullptr) {
            return refuse(QStringLiteral("System Settings is unavailable"));
        }
        return m_controllers.systemMenu->openSettings()
                   ? accept()
                   : refuse(m_controllers.systemMenu->feedback());
    case DesktopCommand::LockScreen:
        return invokeSession("requestLock", QStringLiteral("Locking is unavailable"));
    case DesktopCommand::LogOut:
        return invokeSession("requestLogout", QStringLiteral("Logging out is unavailable"));
    case DesktopCommand::Suspend:
        return invokeSession("requestSuspend", QStringLiteral("Suspend is unavailable"));
    case DesktopCommand::Restart:
        return invokeSession("requestReboot", QStringLiteral("Restart is unavailable"));
    case DesktopCommand::ShutDown:
        return invokeSession("requestPowerOff", QStringLiteral("Shut down is unavailable"));
    case DesktopCommand::NewFileManagerWindow:
        return activateEntry(DesktopMenu::fileManagerDesktopEntryId(),
                             QStringLiteral("File Manager could not be opened"));
    case DesktopCommand::Help:
        return activateEntry(helpDesktopEntryId(), QStringLiteral("Help could not be opened"));
    case DesktopCommand::Find:
        if (m_controllers.launcher == nullptr || !m_hosted.launcher) {
            return refuse(QStringLiteral("The launcher is not in this layout"));
        }
        m_controllers.launcher->requestOpen();
        return accept();
    case DesktopCommand::ClipboardHistory:
        if (m_controllers.clipboard == nullptr || !m_hosted.clipboard) {
            return refuse(QStringLiteral("Clipboard history is not in this layout"));
        }
        m_controllers.clipboard->requestOpen();
        return accept();
    case DesktopCommand::NewFolder:
        return requestSurface(SurfaceCommand::NewFolder);
    case DesktopCommand::Paste:
        return requestSurface(SurfaceCommand::Paste);
    case DesktopCommand::SelectAll:
        return requestSurface(SurfaceCommand::SelectAll);
    case DesktopCommand::CleanUp:
        return requestSurface(SurfaceCommand::CleanUp);
    case DesktopCommand::ShowDesktop:
        if (m_controllers.workspaces == nullptr) {
            return refuse(QStringLiteral("Show Desktop is unavailable"));
        }
        return m_controllers.workspaces->toggleShowingDesktop()
                   ? accept()
                   : refuse(m_controllers.workspaces->feedback());
    case DesktopCommand::SwitchWorkspace:
        if (m_controllers.workspaces == nullptr) {
            return refuse(QStringLiteral("Workspaces are unavailable"));
        }
        // The controller's own revision fence refuses a switch requested from a
        // menu built before the workspace list changed.
        return m_controllers.workspaces->switchTo(command.argument, command.revision)
                   ? accept()
                   : refuse(m_controllers.workspaces->feedback());
    case DesktopCommand::GatherOverview:
        if (!m_hooks.toggleGatherOverview) {
            return refuse(QStringLiteral("The gather overview is unavailable"));
        }
        m_hooks.toggleGatherOverview();
        return accept();
    case DesktopCommand::OpenPlace:
        if (m_controllers.places == nullptr) {
            return refuse(QStringLiteral("Places are unavailable"));
        }
        return m_controllers.places->open(command.argument)
                   ? accept()
                   : refuse(m_controllers.places->feedback());
    case DesktopCommand::ShortcutNote:
        if (!m_hooks.toggleShortcutNote) {
            return refuse(QStringLiteral("The shortcut note is unavailable"));
        }
        m_hooks.toggleShortcutNote();
        return accept();
    }
    return refuse(QStringLiteral("Unknown desktop menu command"));
}

bool ShellDesktopMenuTargets::invokeSession(const char *method, const QString &unavailable)
{
    QObject *session = sessionActions();
    bool accepted = false;
    // AGENT-GUARD: the same Q_INVOKABLE the system menu applet's QML calls; the
    // facade repeats its own admission query before any mutation (ADR-0070).
    if (session == nullptr || !QMetaObject::invokeMethod(session, method, Qt::DirectConnection,
                                                         Q_RETURN_ARG(bool, accepted))) {
        return refuse(unavailable);
    }
    if (!accepted) {
        const QString feedback = session->property("feedback").toString();
        return refuse(feedback.isEmpty() ? unavailable : feedback);
    }
    return accept();
}

bool ShellDesktopMenuTargets::openRoute(const QString &page, const QString &destination)
{
    if (!m_hooks.openSettingsRoute) {
        return refuse(QStringLiteral("Settings cannot be opened from here"));
    }
    return m_hooks.openSettingsRoute(page, destination)
               ? accept()
               : refuse(QStringLiteral("Could not open Settings"));
}

bool ShellDesktopMenuTargets::activateEntry(const QString &entryId, const QString &unavailable)
{
    auto *launcher = m_controllers.launcher;
    if (launcher == nullptr) {
        return refuse(unavailable);
    }
    if (!launcher->activate(entryId)) {
        const QString feedback = launcher->feedback();
        return refuse(feedback.isEmpty() ? unavailable : feedback);
    }
    return accept();
}

bool ShellDesktopMenuTargets::requestSurface(SurfaceCommand command)
{
    if (m_controllers.desktopSurface == nullptr ||
        !m_controllers.desktopSurface->request(command)) {
        return refuse(QStringLiteral("Desktop icons are not shown"));
    }
    return accept();
}

bool ShellDesktopMenuTargets::accept()
{
    m_lastFailure.clear();
    return true;
}

bool ShellDesktopMenuTargets::refuse(const QString &message)
{
    m_lastFailure = message.isEmpty() ? QStringLiteral("The request was refused") : message;
    return false;
}

} // namespace QindaQt::Shell
