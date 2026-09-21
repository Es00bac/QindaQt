// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessiondefaults.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QVariant>

namespace QindaQt::Session {
namespace {

void seedMissing(QSettings &settings, const QString &key,
                 const QVariant &value)
{
    if (!settings.contains(key)) {
        settings.setValue(key, value);
    }
}

// The desktop entry KWin launches as its input method (ADR-0204). A private
// session names its own copy through QINDAQT_OSK_DESKTOP_FILE; otherwise the
// installed entry is used, and no seed is written when neither exists.
QString onScreenKeyboardDesktopFile()
{
    if (qEnvironmentVariableIsSet("QINDAQT_OSK_DESKTOP_FILE")) {
        const QString explicitPath = qEnvironmentVariable("QINDAQT_OSK_DESKTOP_FILE");
        return QFileInfo(explicitPath).isFile() ? explicitPath : QString();
    }
    return QStandardPaths::locate(QStandardPaths::ApplicationsLocation,
                                  QStringLiteral("org.qindaqt.OnScreenKeyboard.desktop"));
}


} // namespace

bool SessionDefaults::ensure(const QString &configHome, QString *error)
{
    if (configHome.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("the configuration home is empty");
        }
        return false;
    }

    QDir directory(configHome);
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error) {
            *error = QStringLiteral("could not create configuration directory '%1'")
                         .arg(configHome);
        }
        return false;
    }

    QSettings kwin(directory.filePath(QStringLiteral("kwinrc")), QSettings::IniFormat);
    kwin.beginGroup(QStringLiteral("org.kde.kdecoration2"));
    // AGENT-CONTRACT: `org.qindaqt` is the installed KDecoration3 module
    // name. First-run defaults select the coherent QindaQt desktop without
    // turning each launcher invocation into an edit of the user's choices.
    seedMissing(kwin, QStringLiteral("library"), QStringLiteral("org.qindaqt"));
    kwin.endGroup();

    kwin.beginGroup(QStringLiteral("Windows"));
    // AGENT-GUARD: An ordinary drag remains a floating-window move. The
    // explicit Meta+Shift path owns QindaQt edge/corner docking; KWin's absent
    // defaults otherwise add a second, conflicting pointer interpretation.
    seedMissing(kwin, QStringLiteral("ElectricBorderTiling"), false);
    seedMissing(kwin, QStringLiteral("ElectricBorderMaximize"), false);
    kwin.endGroup();

    kwin.beginGroup(QStringLiteral("Wayland"));
    // AGENT-CONTRACT: a seed, never an override — a user who chose another
    // virtual keyboard keeps it. KWin reads the entry's Exec line and starts
    // the keyboard on its own input-method connection.
    if (const QString keyboard = onScreenKeyboardDesktopFile(); !keyboard.isEmpty()) {
        seedMissing(kwin, QStringLiteral("InputMethod"), keyboard);
    }
    kwin.endGroup();

    kwin.beginGroup(QStringLiteral("ModifierOnlyShortcuts"));
    // AGENT-CONTRACT: a bare modifier is not a key sequence, so KWin -- not
    // KGlobalAccel -- owns "Meta alone". The entry is KWin's D-Bus call shape:
    // service, path, interface, method, then arguments. This one invokes the
    // shell's own `qindaqt_open_launcher` action
    // (LauncherShortcutProducer::stableActionId), so the Meta key opens the
    // application launcher without the shell owning a bus name of its own.
    // Renaming that action id breaks the key on every machine already seeded.
    // Seed-missing only: a user who bound Meta to something else keeps it.
    seedMissing(kwin, QStringLiteral("Meta"),
                QStringList{QStringLiteral("org.kde.kglobalaccel"),
                            QStringLiteral("/component/qindaqt_shell"),
                            QStringLiteral("org.kde.kglobalaccel.Component"),
                            QStringLiteral("invokeShortcut"),
                            QStringLiteral("qindaqt_open_launcher")});
    kwin.endGroup();

    kwin.beginGroup(QStringLiteral("MouseBindings"));
    // AGENT-CONTRACT (live customization, O9): KWin's default CommandAll3 is
    // "Resize", and its window-action filter runs that command for every
    // client window including layer-shell panels, consuming Meta+right-click
    // before the shell sees it. "Nothing" replays the press to the surface,
    // and KWin already sends keyboard modifiers to the pointer-focused
    // surface, so the shell's customization chord arrives with Meta set.
    // Seed-missing only: a user's own MouseBindings choice always wins.
    seedMissing(kwin, QStringLiteral("CommandAll3"), QStringLiteral("Nothing"));
    kwin.endGroup();

    kwin.beginGroup(QStringLiteral("TabBox"));
    // KWin retains switch/focus authority and supplies the native model. The
    // installed package changes presentation only and consumes the
    // compositor's one-representative-per-container skipSwitcher policy.
    seedMissing(kwin, QStringLiteral("LayoutName"), QStringLiteral("qindaqt"));
    kwin.endGroup();

    // Theming v2 (ADR-0206): translucent panel, popup, chrome and title
    // materials rely on the blur and background-contrast effects. Seeded
    // only when absent, so a user who switched an effect off keeps that.
    kwin.beginGroup(QStringLiteral("Plugins"));
    seedMissing(kwin, QStringLiteral("blurEnabled"), true);
    seedMissing(kwin, QStringLiteral("contrastEnabled"), true);
    kwin.endGroup();
    kwin.beginGroup(QStringLiteral("Effect-blur"));
    seedMissing(kwin, QStringLiteral("BlurStrength"), 8);
    seedMissing(kwin, QStringLiteral("NoiseStrength"), 2);
    kwin.endGroup();

    kwin.beginGroup(QStringLiteral("Effect-overview"));
    // AGENT-CONTRACT (ADR-0232): KWin's overview effect reserves the top-left screen
    // corner by default (its BorderActivate default is ElectricTopLeft = 7,
    // an IntList), so brushing that corner raises KWin's own window grid.
    // QindaQt owns that gesture: the desktop's own gather action arranges
    // iconified chips, rolled-up container cards, and remaining windows in
    // one deterministic layout, which KWin's grid cannot express. An empty
    // list reserves no corner at all; the effect keeps its own shortcut.
    // Seed-missing only, so a user who reassigned the corner keeps it.
    seedMissing(kwin, QStringLiteral("BorderActivate"), QStringList{});
    kwin.endGroup();

    kwin.sync();
    if (kwin.status() != QSettings::NoError) {
        if (error) {
            *error = QStringLiteral("could not persist QindaQt session defaults in '%1'")
                         .arg(kwin.fileName());
        }
        return false;
    }
    return true;
}

} // namespace QindaQt::Session
