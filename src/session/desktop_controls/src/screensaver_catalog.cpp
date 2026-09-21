// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session/desktop_controls/screensaver_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTextStream>

#include <utility>

namespace QindaQt::Session::DesktopControls {

std::optional<ScreensaverCatalogEntry>
ScreensaverCatalog::entry(const QString &token) const {
    const QList<ScreensaverCatalogEntry> all = entries();
    for (const ScreensaverCatalogEntry &candidate : all) {
        if (candidate.token == token) {
            return candidate;
        }
    }
    return std::nullopt;
}

namespace {

constexpr auto kEntryGroup = "Desktop Entry";
constexpr auto kActionGroupPrefix = "Desktop Action ";

// The minimal raw read of a .desktop file: exact unlocalized keys of the
// [Desktop Entry] group plus the Exec/Name of every [Desktop Action ...]
// group. Locale variants and every other group are ignored on purpose; the
// catalog answers "is this a screensaver, and what is its program", nothing
// else.
struct RawDesktopEntry {
    QString type;
    QString name;
    QString genericName;
    QString comment;
    QString iconName;
    QString keywords;
    QString exec;
    bool hidden = false;
    QList<QPair<QString, QString>> actions; // (Name, Exec) per action group
};

bool parseGroupHeader(const QString &line, QString *group) {
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(QLatin1Char('['))
        || !trimmed.endsWith(QLatin1Char(']'))) {
        return false;
    }
    *group = trimmed.mid(1, trimmed.size() - 2);
    return true;
}

RawDesktopEntry readDesktopEntry(const QString &path) {
    RawDesktopEntry entry;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return entry;
    }
    QTextStream stream(&file);
    QString group;
    QString actionName;
    QString actionExec;
    const auto flushAction = [&entry, &actionName, &actionExec] {
        if (!actionExec.isEmpty() || !actionName.isEmpty()) {
            entry.actions.append({actionName, actionExec});
        }
        actionName.clear();
        actionExec.clear();
    };
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        QString parsedGroup;
        if (parseGroupHeader(line, &parsedGroup)) {
            if (group.startsWith(QLatin1String(kActionGroupPrefix))) {
                flushAction();
            }
            group = parsedGroup;
            continue;
        }
        const qsizetype equals = line.indexOf(QLatin1Char('='));
        if (equals < 0) {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();
        if (group == QLatin1String(kEntryGroup)) {
            if (key == QLatin1String("Type")) {
                entry.type = value;
            } else if (key == QLatin1String("Name")) {
                entry.name = value;
            } else if (key == QLatin1String("GenericName")) {
                entry.genericName = value;
            } else if (key == QLatin1String("Comment")) {
                entry.comment = value;
            } else if (key == QLatin1String("Icon")) {
                entry.iconName = value;
            } else if (key == QLatin1String("Keywords")) {
                entry.keywords = value;
            } else if (key == QLatin1String("Exec")) {
                entry.exec = value;
            } else if (key == QLatin1String("Hidden")) {
                entry.hidden =
                    value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
            }
        } else if (group.startsWith(QLatin1String(kActionGroupPrefix))) {
            if (key == QLatin1String("Name")) {
                actionName = value;
            } else if (key == QLatin1String("Exec")) {
                actionExec = value;
            }
        }
    }
    if (group.startsWith(QLatin1String(kActionGroupPrefix))) {
        flushAction();
    }
    return entry;
}

// First word of an Exec line, reduced to the bare program name. The house
// entries are all `Exec=<program> <flags>`; field codes (%U and friends) are
// not part of any saver entry, so they are not interpreted here.
QString programName(const QString &exec) {
    const QString first = exec.split(QLatin1Char(' '), Qt::SkipEmptyParts).value(0);
    QString name = QFileInfo(first).fileName();
    if (name.startsWith(QLatin1Char('"')) && name.endsWith(QLatin1Char('"'))
        && name.size() > 1) {
        name = name.mid(1, name.size() - 2);
    }
    return name;
}

bool attestsScreensaverPurpose(const RawDesktopEntry &entry) {
    const auto mentions = [](const QString &text) {
        return text.contains(QLatin1String("screensaver"), Qt::CaseInsensitive);
    };
    if (mentions(entry.keywords) || mentions(entry.genericName)
        || mentions(entry.comment)) {
        return true;
    }
    for (const auto &action : entry.actions) {
        if (mentions(action.first)) {
            return true;
        }
    }
    return false;
}

bool provesLaunchContract(const RawDesktopEntry &entry, const QString &program) {
    for (const auto &action : entry.actions) {
        if (programName(action.second) != program) {
            continue;
        }
        const QStringList fields =
            action.second.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.contains(QStringLiteral("--screensaver"))
            || fields.contains(QStringLiteral("--all-screens"))) {
            return true;
        }
    }
    return false;
}

} // namespace

DesktopEntryScreensaverCatalog::DesktopEntryScreensaverCatalog()
    : m_directories(systemApplicationDirectories()) {}

DesktopEntryScreensaverCatalog::DesktopEntryScreensaverCatalog(
    QStringList applicationDirectories)
    : m_directories(std::move(applicationDirectories)) {}

QStringList DesktopEntryScreensaverCatalog::systemApplicationDirectories() {
    const QString writable = QStandardPaths::writableLocation(
        QStandardPaths::ApplicationsLocation);
    QStringList directories = QStandardPaths::standardLocations(
        QStandardPaths::ApplicationsLocation);
    // AGENT-GUARD: the user-writable location is never scanned; see the
    // class-level note for why persistence must not name a user-planted file.
    directories.removeAll(writable);
    return directories;
}

QStringList DesktopEntryScreensaverCatalog::launchArguments(const QString &token) {
    // AGENT-CONTRACT: identical to the launcher's pre-discovery command lines,
    // asserted as a set in tst_settings1_screensaver_preferences. Every saver
    // documents --screensaver as covering every connected output; the second
    // flag keeps an unattended screen from reporting metrics or making sound.
    if (token == QLatin1String("qinda-patrol")) {
        return {QStringLiteral("--screensaver"), QStringLiteral("--no-metrics")};
    }
    if (token == QLatin1String("circuit-reef")) {
        return {QStringLiteral("--screensaver"), QStringLiteral("--private")};
    }
    if (token == QLatin1String("prism-circuit")
        || token == QLatin1String("prism-brawl")) {
        return {QStringLiteral("--screensaver"), QStringLiteral("--mute")};
    }
    return {QStringLiteral("--screensaver")};
}

bool DesktopEntryScreensaverCatalog::shipsLockScreenScene(const QString &token) {
    // The studio.qinda.screensaver wallpaper plugin has one scene file per
    // drawable saver; adding one is a QML module in the saver package plus a
    // scene here, so this table cannot be derived from discovery.
    return token == QLatin1String("qinda-patrol")
        || token == QLatin1String("circuit-reef");
}

QList<ScreensaverCatalogEntry> DesktopEntryScreensaverCatalog::entries() const {
    QList<ScreensaverCatalogEntry> found;
    QStringList seenTokens;
    for (const QString &directoryPath : m_directories) {
        const QDir directory(directoryPath);
        const QStringList files = directory.entryList(
            {QStringLiteral("*.desktop")}, QDir::Files, QDir::Name);
        for (const QString &file : files) {
            const RawDesktopEntry raw =
                readDesktopEntry(directory.absoluteFilePath(file));
            if (raw.type != QLatin1String("Application") || raw.hidden
                || raw.exec.isEmpty()) {
                continue;
            }
            const QString token = programName(raw.exec);
            // "none" and "blank" are reserved tokens owned by Settings, never
            // names a real program may take here.
            if (token.isEmpty() || token == QLatin1String("none")
                || token == QLatin1String("blank") || seenTokens.contains(token)) {
                continue;
            }
            if (!attestsScreensaverPurpose(raw)
                || !provesLaunchContract(raw, token)) {
                continue;
            }
            ScreensaverCatalogEntry entry;
            entry.token = token;
            entry.name = raw.name.isEmpty() ? token : raw.name;
            entry.comment =
                raw.comment.isEmpty() ? raw.genericName : raw.comment;
            entry.iconName = raw.iconName;
            entry.arguments = launchArguments(token);
            entry.showsOnLockScreen = shipsLockScreenScene(token);
            seenTokens.append(token);
            found.append(entry);
        }
    }
    return found;
}

} // namespace QindaQt::Session::DesktopControls
