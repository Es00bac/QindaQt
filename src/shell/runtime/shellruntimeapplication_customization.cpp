// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include "kglobalaccelshortcutregistrar.h"
#include "livecustomizationcontroller.h"
#include "livecustomizationshortcut.h"
#include "runtimepanelwindowfactory.h"
#include "settingsroutelauncher.h"

#include "qindaqt/shell_surface/qt_output_inventory.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace QindaQt::Shell {

namespace {

QString writableUserProfileDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(QStringLiteral("qindaqt/profiles"));
}

} // namespace

void ShellRuntimeApplication::ensureUserProfileStoreInCatalog()
{
    // AGENT-CONTRACT: catalog directories merge low-to-high with the user
    // store last (loadCatalogs). A store created after startup by the first
    // Apply -- from a menu here or from the Settings Customize route -- must
    // join the list, or every reload keeps re-reading the built-in copy.
    if (m_profileLockedByCli) {
        return;
    }
    const QString userProfiles = writableUserProfileDirectory();
    if (QDir(userProfiles).exists() && !m_profileCatalogDirectories.contains(userProfiles)) {
        m_profileCatalogDirectories.append(userProfiles);
    }
}

void ShellRuntimeApplication::initializeLiveCustomization(
    const Profiles::LayoutProfile &profile)
{
    // AGENT-CONTRACT: the user profile directory is exactly the directory
    // refreshProfileStoreWatch() watches and the Settings Customize route
    // writes, so an Apply from a menu entry surfaces as an ordinary layout
    // adoption and the parity invariant holds by construction.
    m_liveCustomization = std::make_unique<LiveCustomizationController>(
        m_applets.manifests(), writableUserProfileDirectory(),
        [] { return ShellSurface::QtOutputInventory::read().outputs; },
        m_customizationSettingsClient.get(), m_settingsRouteLauncher.get());
    m_liveCustomization->adoptProfile(profile);
    // AGENT-CONTRACT: every accepted menu action or drop has already written
    // the user store; adopt it now rather than waiting for the store watcher,
    // which cannot fire for the very first write on a machine whose
    // qindaqt/profiles directory did not exist at shell start (the watcher
    // only learns the directory on the next refresh). The debounced adoption
    // re-arms the watcher, and an adoption of the profile the session just
    // committed keeps the session and its undo history.
    connect(m_liveCustomization.get(), &LiveCustomizationController::actionReported, this,
            [this](bool ok, const QString &, const QString &) {
                if (ok) {
                    m_profileAdoptDebounce.start();
                }
            });
    if (m_windowFactory) {
        m_windowFactory->setLiveCustomization(m_liveCustomization.get());
    }
    // AGENT-NOTE: registration completes synchronously inside the shortcut's
    // constructor; the stack registrar may die at scope exit (note precedent).
    KGlobalAccelShortcutRegistrar registrar;
    m_liveCustomizationShortcut = std::make_unique<LiveCustomizationShortcut>(
        registrar, [this] {
            if (m_liveCustomization) {
                m_liveCustomization->toggleEditMode();
            }
        });
    if (!m_liveCustomizationShortcut->registrationRequestAccepted()) {
        qWarning().noquote()
            << "QindaQt shell could not submit the panel edit-mode global"
               " shortcut; the menu entry remains available";
    }
}

} // namespace QindaQt::Shell
