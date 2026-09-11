// SPDX-License-Identifier: GPL-3.0-or-later
#include "sessiondefaults.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QSettings>
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

    kwin.beginGroup(QStringLiteral("TabBox"));
    // KWin retains switch/focus authority and supplies the native model. The
    // installed package changes presentation only and consumes the
    // compositor's one-representative-per-container skipSwitcher policy.
    seedMissing(kwin, QStringLiteral("LayoutName"), QStringLiteral("qindaqt"));
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
