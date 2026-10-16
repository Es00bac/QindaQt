// SPDX-License-Identifier: GPL-3.0-or-later
//
// Catalog loading for ShellRuntimeApplication: profile, theme, applet and
// applet-policy catalogs, and the startup selection of a profile and theme
// from them. Split from shellruntimeapplication.cpp like the other
// shellruntimeapplication_*.cpp files so the composition root stays within the
// source-shape policy; the rules implemented here are ADR-0393 and
// docs/wiki/shell/layout-profiles.md ("Catalog directories").
#include "shellruntimeapplication.h"

#include "../common/catalogpaths.h"
#include "../common/shelliconconfiguration.h"
#include "default_layout_profile.h"
#include "shellpreferencevalues.h"

#include "qindaqt/applet_host/capability_policy_loader.h"

#include <QDebug>
#include <QDir>
#include <QProcessEnvironment>

#include <utility>

namespace QindaQt::Shell {

bool ShellRuntimeApplication::loadCatalogs(const RuntimeOptions &options, QString *error)
{
    // AGENT-CONTRACT: Profile catalogs merge low-to-high precedence with the
    // user store last, matching the Settings Customize route contract (project
    // audit A06). The source tree participates only for the genuine
    // build-tree executable so an installed shell never shadows user profiles.
    const QString sourceProfileDirectory = buildTreeSourceDirectory(
        QINDAQT_SOURCE_PROFILE_DIR, QINDAQT_SHELL_BUILD_EXECUTABLE_PATH);
    const QStringList profileDirectories = resolveProfileCatalogDirectories(
        options.profileDirectory, sourceProfileDirectory);
    m_profileCatalogDirectories = profileDirectories;
    m_profileLockedByCli = !options.profileId.isEmpty();
    // AGENT-CONTRACT (ADR-0393): themes, applet manifests and the applet policy
    // merge EVERY XDG data directory, low to high precedence, user last. They
    // used to take the first directory QStandardPaths::locate found, so a stale
    // ~/.local/share/qindaqt/themes (19 symlinks from September) hid all 28
    // installed themes and the saved theme was rejected at every login.
    const QStringList themeDirectories = resolveCatalogDataDirectories(
        options.themeDirectory, "QINDAQT_THEME_DIR", QINDAQT_SOURCE_THEME_DIR,
        QStringLiteral("qindaqt/themes"));
    const QStringList appletDirectories = resolveCatalogDataDirectories(
        options.appletDirectory, "QINDAQT_APPLET_DIR", QINDAQT_SOURCE_APPLET_DIR,
        QStringLiteral("qindaqt/applets"));
    const QStringList appletPolicyFiles = resolveCatalogDataFiles(
        options.appletPolicyFile, "QINDAQT_APPLET_POLICY",
        QINDAQT_SOURCE_APPLET_POLICY,
        QStringLiteral("qindaqt/applet-policy/default.json"));
    m_themeCatalogDirectories = themeDirectories;
    QStringList skippedEntries;
    if (!m_profiles.loadDirectories(profileDirectories, error) ||
        !m_themes.loadDirectories(themeDirectories, error, &skippedEntries) ||
        !m_applets.loadDirectories(appletDirectories, error, &skippedEntries)) {
        return false;
    }
    const auto loadedPolicy = AppletHost::CapabilityPolicyLoader::fromFiles(
        appletPolicyFiles, &skippedEntries);
    if (!loadedPolicy.ok) {
        *error = loadedPolicy.error;
        return false;
    }
    m_appletPolicy = loadedPolicy.policy;
    for (const QString &problem : std::as_const(skippedEntries)) {
        // An override layer (normally the user's data directory) held an entry
        // that did not validate. It is ignored; the installed entry, if any,
        // stays in force.
        qWarning().noquote()
            << "QindaQt shell ignored an invalid override catalog entry:" << problem;
    }
    qInfo().noquote() << "QindaQt shell catalog directories, low to high precedence:"
                      << "themes=" << themeDirectories.join(QLatin1Char(':'))
                      << "applets=" << appletDirectories.join(QLatin1Char(':'))
                      << "policy=" << appletPolicyFiles.join(QLatin1Char(':'));
    const QString requestedProfile =
        resolveStartupProfileId(options.profileId, m_startupPreferences);
    if (!m_profiles.selectById(requestedProfile)) {
        // AGENT-GUARD: Only an explicit --profile may fail startup. A saved
        // selection whose profile was deleted or renamed must recover to the
        // built-in default instead of stranding the session without a shell.
        if (!options.profileId.isEmpty() || !m_startupPreferences.has_value()) {
            *error = QStringLiteral("Unknown profile: %1").arg(requestedProfile);
            return false;
        }
        const QString fallback = QString::fromLatin1(DefaultLayoutProfileId);
        // AGENT-NOTE: names the requested AND the fallback profile. The
        // phrase "falling back to the default profile" is pinned by
        // tst_shellruntime_startuplaunch.
        qWarning().noquote()
            << QStringLiteral("QindaQt shell could not honor the saved profile"
                              " selection; falling back to the default profile"
                              " '%1': saved profile '%2' is not in the profile"
                              " catalogs")
                   .arg(fallback, requestedProfile);
        if (!m_profiles.selectById(fallback)) {
            *error = QStringLiteral("Unknown profile: %1").arg(fallback);
            return false;
        }
    }

    const QString profileDefaultTheme =
        m_profiles.current().value(QStringLiteral("defaultTheme")).toString();
    QString requestedTheme = resolveStartupThemeId(
        options.themeId, m_startupPreferences, profileDefaultTheme);
    if (!m_themes.selectById(requestedTheme)) {
        // AGENT-GUARD: Only an explicit --theme may fail startup. A saved
        // preference naming an uninstalled theme fails closed to the selected
        // profile's default rather than leaving the session without a shell.
        if (!options.themeId.isEmpty() || !m_startupPreferences.has_value()
            || requestedTheme == profileDefaultTheme) {
            *error = QStringLiteral("Unknown theme: %1").arg(requestedTheme);
            return false;
        }
        // AGENT-NOTE: name BOTH themes and the directories searched. This
        // warning used to print only the requested theme while claiming to
        // name the fallback, which hid that the catalog (not the preference)
        // was wrong when a stale directory shadowed the installed themes.
        qWarning().noquote()
            << QStringLiteral("QindaQt shell could not honor the saved theme '%1'"
                              " (not in the theme catalogs searched: %2);"
                              " falling back to the profile default '%3'")
                   .arg(requestedTheme,
                        m_themeCatalogDirectories.join(QLatin1Char(':')),
                        profileDefaultTheme);
        requestedTheme = profileDefaultTheme;
        if (!m_themes.selectById(requestedTheme)) {
            *error = QStringLiteral("Unknown theme: %1").arg(requestedTheme);
            return false;
        }
    }
    m_dataRoots = ShellIconConfiguration::dataRoots(
        QProcessEnvironment::systemEnvironment(), QDir::homePath());
    refreshProfileStoreWatch();
    return true;
}

} // namespace QindaQt::Shell
