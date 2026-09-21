// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/winelaunchpaths.h"

#include <QDir>

namespace QindaQt::Compositor {
namespace {

constexpr qsizetype maximumCommandLineBytes = 8192;
constexpr qsizetype maximumPathCharacters = 1024;
constexpr qsizetype maximumEnvironBytes = 65536;

[[nodiscard]] bool isUsablePath(const QString &value)
{
    if (value.isEmpty() || value.size() > maximumPathCharacters) {
        return false;
    }
    for (const QChar character : value) {
        const char16_t code = character.unicode();
        if (code <= 0x001F || code == 0x007F) {
            return false;
        }
    }
    return true;
}

// The drive letter for `X:`-prefixed paths, uppercased, or 0 when the path is
// not drive-prefixed.
[[nodiscard]] QChar driveLetterOf(const QString &path)
{
    if (path.size() < 2 || path.at(1) != QLatin1Char(':')) {
        return {};
    }
    const QChar letter = path.at(0).toUpper();
    return letter >= QLatin1Char('A') && letter <= QLatin1Char('Z') ? letter
                                                                    : QChar{};
}

} // namespace

QString windowsExecutablePathFromCommandLine(const QByteArray &cmdline)
{
    if (cmdline.isEmpty() || cmdline.size() > maximumCommandLineBytes) {
        return {};
    }
    const QList<QByteArray> arguments = cmdline.split('\0');
    QString lastWindowsExecutable;
    for (const QByteArray &argument : arguments) {
        if (argument.isEmpty()) {
            continue;
        }
        const QString candidate =
            QString::fromLocal8Bit(argument).trimmed();
        if (!isUsablePath(candidate)) {
            continue;
        }
        if (candidate.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            lastWindowsExecutable = candidate;
        }
    }
    // AGENT-GUARD: as with the ADR-0169 basename rule, the LAST `.exe` wins -
    // launcher shims precede the real program on a Wine/Proton command line.
    return lastWindowsExecutable;
}

QString winePrefixFromEnviron(const QByteArray &environ, const QString &homeDir)
{
    if (environ.size() > maximumEnvironBytes) {
        return {};
    }
    const QList<QByteArray> variables = environ.split('\0');
    for (const QByteArray &variable : variables) {
        if (!variable.startsWith(QByteArrayLiteral("WINEPREFIX="))) {
            continue;
        }
        const QString value =
            QString::fromLocal8Bit(variable.mid(sizeof("WINEPREFIX=") - 1));
        if (!isUsablePath(value) || !value.startsWith(QLatin1Char('/'))) {
            // Present but unusable: fail closed rather than guess a tree.
            return {};
        }
        return value;
    }
    return QDir::cleanPath(homeDir + QStringLiteral("/.wine"));
}

QString hostPathForWindowsExecutable(const QString &windowsPath,
                                     const QString &winePrefix)
{
    if (!isUsablePath(windowsPath) || winePrefix.isEmpty()) {
        return {};
    }
    if (windowsPath.startsWith(QLatin1Char('/'))) {
        // Already a host path (Proton launchers hand these to Wine verbatim).
        return QDir::cleanPath(windowsPath);
    }
    const QChar drive = driveLetterOf(windowsPath);
    if (drive.isNull()) {
        return {};
    }
    QString rest = windowsPath.mid(2);
    rest.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (rest.startsWith(QLatin1Char('/'))) {
        rest.remove(0, 1);
    }
    if (drive == QLatin1Char('Z')) {
        // Wine maps Z: to the host root; rest already lost its separators.
        return QDir::cleanPath(QLatin1Char('/') + rest);
    }
    if (drive != QLatin1Char('C')) {
        // Other drive letters live in dosdevices/ and are out of scope: fail
        // closed rather than enumerate user symlinks.
        return {};
    }
    const QString driveRoot =
        QDir::cleanPath(winePrefix + QStringLiteral("/drive_c"));
    const QString mapped = QDir::cleanPath(driveRoot + QLatin1Char('/') + rest);
    // AGENT-GUARD: `C:\..\..` must never escape the prefix. The check is
    // lexical; the caller re-checks after symlink canonicalization.
    if (mapped != driveRoot && !mapped.startsWith(driveRoot + QLatin1Char('/'))) {
        return {};
    }
    return mapped;
}

} // namespace QindaQt::Compositor
