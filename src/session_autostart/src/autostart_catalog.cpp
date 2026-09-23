// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/session_autostart/autostart_catalog.h>

#include <qindaqt/shell_launcher/launch_execution.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>

#include <algorithm>
#include <utility>

namespace QindaQt::SessionAutostart {
namespace {
constexpr qsizetype MaximumDesktopFileBytes = 65'536;
constexpr int MaximumEntries = 1'024;

struct Fields final {
    QString type;
    QString name;
    QString comment;
    QString icon;
    QString exec;
    QString tryExec;
    QStringList onlyShowIn;
    QStringList notShowIn;
    QString phase;
    bool hidden = false;
    bool gnomeDisabled = false;
    bool custom = false;
    bool invalidBoolean = false;
    bool hasOnlyShowIn = false;
    bool hasNotShowIn = false;
};

QStringList splitDesktopList(const QString &value)
{
    QStringList result;
    for (const QString &part : value.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        if (!part.trimmed().isEmpty())
            result.append(part.trimmed());
    }
    return result;
}

Fields parseFields(const QString &document)
{
    Fields fields;
    bool inEntry = false;
    for (const QString &line : document.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
            inEntry = trimmed == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inEntry || trimmed.startsWith(QLatin1Char('#')))
            continue;
        const qsizetype separator = line.indexOf(QLatin1Char('='));
        if (separator <= 0)
            continue;
        const QString key = line.left(separator).trimmed();
        const QString value = line.mid(separator + 1).trimmed();
        if (key == QLatin1String("Type")) fields.type = value;
        else if (key == QLatin1String("Name")) fields.name = value;
        else if (key == QLatin1String("Comment")) fields.comment = value;
        else if (key == QLatin1String("Icon")) fields.icon = value;
        else if (key == QLatin1String("Exec")) fields.exec = value;
        else if (key == QLatin1String("TryExec")) fields.tryExec = value;
        else if (key == QLatin1String("OnlyShowIn")) {
            fields.hasOnlyShowIn = true;
            fields.onlyShowIn = splitDesktopList(value);
        } else if (key == QLatin1String("NotShowIn")) {
            fields.hasNotShowIn = true;
            fields.notShowIn = splitDesktopList(value);
        }
        else if (key == QLatin1String("X-GNOME-Autostart-Phase")) fields.phase = value;
        else if (key == QLatin1String("Hidden")) {
            if (value.compare(QLatin1String("true"), Qt::CaseInsensitive) != 0
                && value.compare(QLatin1String("false"), Qt::CaseInsensitive) != 0)
                fields.invalidBoolean = true;
            fields.hidden = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
        } else if (key == QLatin1String("X-GNOME-Autostart-enabled")) {
            if (value.compare(QLatin1String("true"), Qt::CaseInsensitive) != 0
                && value.compare(QLatin1String("false"), Qt::CaseInsensitive) != 0)
                fields.invalidBoolean = true;
            fields.gnomeDisabled = value.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0;
        }
        else if (key == QLatin1String("X-QindaQt-Custom")) fields.custom = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
    }
    return fields;
}

QString resolveExecutable(const QString &program, const QStringList &directories)
{
    if (program.isEmpty())
        return {};
    if (QFileInfo(program).isAbsolute()) {
        const QFileInfo file(program);
        return file.isFile() && file.isExecutable() ? program : QString{};
    }
    if (program.contains(QLatin1Char('/')))
        return {};
    for (const QString &directory : directories) {
        const QString path = QDir(directory).filePath(program);
        const QFileInfo file(path);
        if (file.isFile() && file.isExecutable())
            return file.absoluteFilePath();
    }
    return {};
}

bool intersects(const QStringList &left, const QStringList &right)
{
    for (const QString &value : left) {
        if (right.contains(value))
            return true;
    }
    return false;
}

void markIneligible(Entry &entry, const QString &reason)
{
    entry.ineligibilityReason = reason;
    entry.eligible = false;
}

} // namespace

ScanOptions ScanOptions::fromEnvironment(const QProcessEnvironment &environment)
{
    ScanOptions options;
    const QString configHome = environment.value(QStringLiteral("XDG_CONFIG_HOME"));
    const QString home = environment.value(QStringLiteral("HOME"));
    const QString userRoot = QFileInfo(configHome).isAbsolute()
        ? configHome
        : (QFileInfo(home).isAbsolute()
               ? QDir(home).filePath(QStringLiteral(".config")) : QString{});
    if (!userRoot.isEmpty())
        options.userDirectory = QDir(userRoot).filePath(QStringLiteral("autostart"));
    QString configDirs = environment.value(QStringLiteral("XDG_CONFIG_DIRS"));
    if (configDirs.isEmpty())
        configDirs = QStringLiteral("/etc/xdg");
    for (const QString &root : configDirs.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        if (QFileInfo(root).isAbsolute())
            options.systemDirectories.append(QDir(root).filePath(QStringLiteral("autostart")));
    }
    QString desktop = environment.value(QStringLiteral("XDG_CURRENT_DESKTOP"));
    if (desktop.isEmpty())
        desktop = QStringLiteral("QindaQt");
    options.desktops = desktop.split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &path : environment.value(QStringLiteral("PATH")).split(
             QDir::listSeparator(), Qt::SkipEmptyParts)) {
        if (QFileInfo(path).isAbsolute())
            options.executableDirectories.append(path);
    }
    return options;
}

QList<Entry> scan(const ScanOptions &options, QString *error)
{
    if (error != nullptr)
        error->clear();
    // ID -> absolute source path. First root wins, including an invalid or
    // hidden winner, so a lower-priority entry can never leak through.
    QHash<QString, QString> winners;
    bool capped = false;
    const auto readDirectory = [&winners, &capped](const QString &path) {
        const QDir directory(path);
        for (const QFileInfo &file : directory.entryInfoList(
                 {QStringLiteral("*.desktop")}, QDir::Files, QDir::Name)) {
            const QString id = file.completeBaseName();
            if (winners.contains(id))
                continue;
            if (winners.size() >= MaximumEntries) {
                capped = true;
                continue;
            }
            winners.insert(id, file.absoluteFilePath());
        }
    };
    if (!options.userDirectory.isEmpty())
        readDirectory(options.userDirectory);
    for (const QString &directory : options.systemDirectories)
        readDirectory(directory);
    if (capped && error != nullptr)
        *error = QStringLiteral("autostart entry limit reached; remaining entries were not scanned");

    QList<Entry> entries;
    for (auto it = winners.constBegin(); it != winners.constEnd(); ++it) {
        QFile file(it.value());
        if (!file.open(QIODevice::ReadOnly) || file.size() > MaximumDesktopFileBytes)
            continue;
        const QString document = QString::fromUtf8(file.readAll());
        const Fields fields = parseFields(document);
        if (fields.type != QLatin1String("Application") || fields.name.trimmed().isEmpty())
            continue;
        Entry entry;
        entry.id = it.key();
        entry.sourcePath = it.value();
        entry.name = fields.name;
        entry.comment = fields.comment;
        entry.iconName = fields.icon;
        entry.exec = fields.exec;
        entry.custom = fields.custom;
        entry.enabled = !fields.hidden && !fields.gnomeDisabled;
        if (fields.invalidBoolean) {
            markIneligible(entry, QStringLiteral("Invalid desktop autostart flag"));
        } else if (fields.hasOnlyShowIn && fields.hasNotShowIn) {
            markIneligible(entry, QStringLiteral("Conflicting desktop conditions"));
        } else if (!entry.enabled) {
            markIneligible(entry, fields.hidden ? QStringLiteral("Disabled for this user")
                                               : QStringLiteral("Disabled by a desktop autostart flag"));
        } else if (fields.hasOnlyShowIn && !intersects(fields.onlyShowIn, options.desktops)) {
            markIneligible(entry, QStringLiteral("Only starts in %1").arg(fields.onlyShowIn.join(QStringLiteral(", "))));
        } else if (intersects(fields.notShowIn, options.desktops)) {
            markIneligible(entry, QStringLiteral("Excluded from this desktop"));
        } else if (!fields.phase.isEmpty() && fields.phase != QLatin1String("Application")) {
            markIneligible(entry, QStringLiteral("Requires an unsupported desktop startup phase"));
        } else if (!fields.tryExec.isEmpty()
                   && resolveExecutable(fields.tryExec, options.executableDirectories).isEmpty()) {
            markIneligible(entry, QStringLiteral("Required executable is unavailable: %1").arg(fields.tryExec));
        } else {
            const auto parsed = Shell::Launcher::LaunchExecutionParser::parse(document);
            if (!parsed.ok()) {
                markIneligible(entry, parsed.message);
            } else if (parsed.keys->dbusActivatable) {
                markIneligible(entry, QStringLiteral("D-Bus activation is not supported by this session"));
            } else {
                const auto plan = Shell::Launcher::ExecFieldCodeExpander::expand(
                    parsed.keys->exec, {fields.name, fields.icon, entry.sourcePath});
                if (!plan.ok()) {
                    markIneligible(entry, plan.message);
                } else {
                    const QString program = resolveExecutable(plan.plan->program, options.executableDirectories);
                    const QString workingDirectory = parsed.keys->path;
                    if (program.isEmpty()) {
                        markIneligible(entry, QStringLiteral("Command is unavailable: %1").arg(plan.plan->program));
                    } else if (!workingDirectory.isEmpty()
                               && (!QFileInfo(workingDirectory).isAbsolute()
                                   || !QFileInfo(workingDirectory).isDir())) {
                        markIneligible(entry, QStringLiteral("Working directory is unavailable"));
                    } else if (parsed.keys->terminal) {
                        const QString terminal = resolveExecutable(
                            options.terminalExecutable, options.executableDirectories);
                        if (terminal.isEmpty()) {
                            markIneligible(entry, QStringLiteral("Terminal is unavailable"));
                        } else {
                            entry.program = terminal;
                            entry.arguments = {QStringLiteral("-e"), program};
                            entry.arguments.append(plan.plan->arguments);
                            entry.workingDirectory = workingDirectory;
                            entry.eligible = true;
                        }
                    } else {
                        entry.program = program;
                        entry.arguments = plan.plan->arguments;
                        entry.workingDirectory = workingDirectory;
                        entry.eligible = true;
                    }
                }
            }
        }
        entries.append(std::move(entry));
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return entries;
}

} // namespace QindaQt::SessionAutostart
