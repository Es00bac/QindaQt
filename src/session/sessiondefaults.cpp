// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessiondefaults.h"

#include "qindaqt/compositor_names/compositor_names.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaType>
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

    // qindaqt-kwin reads its settings from QindaQt's config folder (ADR-0289).
    const QString compositorConfig = directory.filePath(QString(CompositorNames::configFile));
    if (!QDir().mkpath(QFileInfo(compositorConfig).absolutePath())) {
        if (error) {
            *error = QStringLiteral("could not create configuration directory '%1'")
                         .arg(QFileInfo(compositorConfig).absolutePath());
        }
        return false;
    }
    QSettings kwin(compositorConfig, QSettings::IniFormat);
    // AGENT-NOTE (ADR-0289): the QindaQt decoration, the qindaqt window
    // switcher, disabled electric-border maximize and tiling, CommandAll3
    // "Nothing" (the shell's Meta+right-click chord) and the Theming v2 blur
    // strengths are qindaqt-kwin's compiled-in defaults, so they are not
    // seeded here; only values that depend on this installation or on
    // QindaQt's own components are.

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


    kwin.beginGroup(QStringLiteral("Effect-overview"));
    // AGENT-GUARD (ADR-0240): KWin reads this entry as an IntList. QSettings
    // serializes an empty QStringList as @Invalid(), which KConfig reads as
    // edge 0 (ElectricTop), making KWin's overview own the entire top edge.
    // A valid empty QString persists as BorderActivate=, an empty IntList.
    // Repair that legacy seed whether QSettings has reloaded it as invalid or
    // still has the typed empty list cached in this process. Preserve a user's
    // chosen edges.
    const QString overviewBorder = QStringLiteral("BorderActivate");
    const QVariant existingOverviewBorder = kwin.value(overviewBorder);
    const bool malformedEmptyList =
        existingOverviewBorder.metaType() == QMetaType::fromType<QStringList>()
        && existingOverviewBorder.toStringList().isEmpty();
    if (!kwin.contains(overviewBorder) || !existingOverviewBorder.isValid()
        || malformedEmptyList) {
        kwin.setValue(overviewBorder, QStringLiteral(""));
    }
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
