// SPDX-License-Identifier: GPL-3.0-or-later
#include "polkit_requester_resolver.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <utility>

namespace QindaQt::Apps::PolkitAgent {
namespace {

QString readFirstLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readLine()).trimmed();
}

// A minimal, display-only *.desktop reader: just Name= and Icon= for the
// entry whose Exec= program token's basename matches. This is deliberately
// not the launch-eligibility parser session_autostart owns; a wrong or
// missing match only ever costs a nicer label, never a launch decision.
struct DesktopEntryHint final {
    QString name;
    QString icon;
    bool found = false;
};

DesktopEntryHint findDesktopHintFor(const QString &programBasename,
                                    const QStringList &applicationDirectories)
{
    for (const QString &directory : applicationDirectories) {
        const QDir dir(directory);
        if (!dir.exists()) {
            continue;
        }
        const auto files = dir.entryList({QStringLiteral("*.desktop")}, QDir::Files);
        for (const QString &fileName : files) {
            QFile file(dir.filePath(fileName));
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            QString name;
            QString icon;
            QString execBasename;
            bool inEntry = false;
            while (!file.atEnd()) {
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
                    inEntry = line == QLatin1String("[Desktop Entry]");
                    continue;
                }
                if (!inEntry) {
                    continue;
                }
                const qsizetype separator = line.indexOf(QLatin1Char('='));
                if (separator <= 0) {
                    continue;
                }
                const QString key = line.left(separator).trimmed();
                const QString value = line.mid(separator + 1).trimmed();
                if (key == QLatin1String("Name") && name.isEmpty()) {
                    name = value;
                } else if (key == QLatin1String("Icon") && icon.isEmpty()) {
                    icon = value;
                } else if (key == QLatin1String("Exec") && execBasename.isEmpty()) {
                    const QString token = value.section(QLatin1Char(' '), 0, 0);
                    execBasename = QFileInfo(token).fileName();
                }
            }
            if (!execBasename.isEmpty() && execBasename == programBasename) {
                return {name, icon, true};
            }
        }
    }
    return {};
}

} // namespace

PolkitRequesterResolver::PolkitRequesterResolver(QString procRoot,
                                                 QStringList applicationDirectories)
    : m_procRoot(std::move(procRoot))
    , m_applicationDirectories(std::move(applicationDirectories))
{
}

RequesterInfo PolkitRequesterResolver::resolve(qint64 pid) const
{
    if (pid <= 0) {
        return {};
    }
    const QString processDirectory = QStringLiteral("%1/%2").arg(m_procRoot).arg(pid);
    const QFileInfo exeLink(processDirectory + QStringLiteral("/exe"));
    const QString programPath =
        exeLink.exists() || exeLink.isSymLink() ? exeLink.symLinkTarget() : QString();
    const QString comm = readFirstLine(processDirectory + QStringLiteral("/comm"));
    if (programPath.isEmpty() && comm.isEmpty()) {
        return {};
    }
    RequesterInfo info;
    info.programPath = programPath;
    const QString basename =
        programPath.isEmpty() ? comm : QFileInfo(programPath).fileName();
    const DesktopEntryHint hint = findDesktopHintFor(basename, m_applicationDirectories);
    if (hint.found && !hint.name.isEmpty()) {
        info.displayName = hint.name;
        info.iconName = hint.icon;
    } else {
        info.displayName = programPath.isEmpty() ? comm : programPath;
    }
    return info;
}

} // namespace QindaQt::Apps::PolkitAgent
