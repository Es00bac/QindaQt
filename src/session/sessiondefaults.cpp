// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessiondefaults.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QVariant>

namespace QindaQt::Session {
namespace {

constexpr auto directoryHandlerEntry =
    "inode/directory=org.qindaqt.FileManager.desktop";

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

bool parseSection(const QString &line, QString *section)
{
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QLatin1Char('[')) || !trimmed.endsWith(QLatin1Char(']'))) {
        return false;
    }
    *section = trimmed.mid(1, trimmed.size() - 2);
    return true;
}

// AGENT-CONTRACT: this is a seed-missing default, never an override. A user
// or distribution choice for inode/directory in [Default Applications] always
// wins, and an unusable seed must never block session start, so write
// failures are reported and ignored.
void seedMissingDirectoryHandler(const QDir &configHome)
{
    const auto path = configHome.filePath(QStringLiteral("mimeapps.list"));
    QString contents;
    if (QFile file(path); file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        contents = QString::fromUtf8(file.readAll());
    }
    QStringList lines = contents.split(QLatin1Char('\n'));
    QString section;
    int defaultsHeader = -1;
    for (int index = 0; index < lines.size(); ++index) {
        QString parsed;
        if (parseSection(lines.at(index), &parsed)) {
            section = parsed;
            if (section == QLatin1String("Default Applications")) {
                defaultsHeader = index;
            }
            continue;
        }
        if (section != QLatin1String("Default Applications")) {
            continue;
        }
        const QString entry = lines.at(index).section(QLatin1Char('='), 0, 0).trimmed();
        if (entry == QLatin1String("inode/directory")) {
            return;
        }
    }

    const QString line = QLatin1String(directoryHandlerEntry);
    if (defaultsHeader >= 0) {
        lines.insert(defaultsHeader + 1, line);
    } else {
        if (!lines.isEmpty() && !lines.constLast().trimmed().isEmpty()) {
            lines.append(QString());
        }
        lines.append(QStringLiteral("[Default Applications]"));
        lines.append(line);
    }

    QSaveFile save(path);
    if (!save.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning("QindaQt session could not seed the directory handler default in '%s'",
                 qPrintable(path));
        return;
    }
    QTextStream stream(&save);
    stream.setEncoding(QStringConverter::Utf8);
    stream << lines.join(QLatin1Char('\n'));
    if (!save.commit()) {
        qWarning("QindaQt session could not persist the directory handler default in '%s'",
                 qPrintable(path));
    }
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

    kwin.sync();
    if (kwin.status() != QSettings::NoError) {
        if (error) {
            *error = QStringLiteral("could not persist QindaQt session defaults in '%1'")
                         .arg(kwin.fileName());
        }
        return false;
    }
    seedMissingDirectoryHandler(directory);
    return true;
}

} // namespace QindaQt::Session
