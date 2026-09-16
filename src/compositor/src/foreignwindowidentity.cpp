// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/compositor/foreignwindowidentity.h"

#include <QList>
#include <QRegularExpression>

namespace QindaQt::Compositor {
namespace {

// A command line is attacker-influenced input from another process. Read a
// bounded prefix and refuse anything longer rather than scanning without limit.
constexpr qsizetype maximumCommandLineBytes = 8192;
constexpr qsizetype maximumExecutableCharacters = 128;

// Wine's own helper classes. A window reporting one of these is never the
// application the user launched.
[[nodiscard]] bool isWineShimClass(const QString &lowered)
{
    static const QList<QString> shims{
        QStringLiteral("wine"),          QStringLiteral("explorer.exe"),
        QStringLiteral("wineboot.exe"),  QStringLiteral("winedbg.exe"),
        QStringLiteral("rundll32.exe"),  QStringLiteral("start.exe"),
        QStringLiteral("services.exe"),  QStringLiteral("winemenubuilder.exe"),
    };
    return shims.contains(lowered);
}

[[nodiscard]] QString basenameOf(const QString &pathLike)
{
    // Wine reports Windows paths; a native client reports a POSIX path.
    const qsizetype lastBackslash = pathLike.lastIndexOf(QLatin1Char('\\'));
    const qsizetype lastSlash = pathLike.lastIndexOf(QLatin1Char('/'));
    const qsizetype cut = std::max(lastBackslash, lastSlash);
    return cut < 0 ? pathLike : pathLike.mid(cut + 1);
}

[[nodiscard]] bool isAcceptableExecutable(const QString &value)
{
    if (value.isEmpty() || value.size() > maximumExecutableCharacters) {
        return false;
    }
    for (const QChar character : value) {
        const char16_t code = character.unicode();
        // Control characters would reach presentation and a path separator
        // would mean the basename split failed.
        if (code <= 0x001F || code == 0x007F || character == QLatin1Char('/')
            || character == QLatin1Char('\\')) {
            return false;
        }
    }
    return true;
}

} // namespace

bool isOpaqueLauncherClass(const QString &resourceClass)
{
    const QString trimmed = resourceClass.trimmed();
    if (trimmed.isEmpty()) {
        return true;
    }
    const QString lowered = trimmed.toLower();
    // AGENT-NOTE: `steam_app_0` is the umu/non-Steam case and carries no
    // identity at all; `steam_app_<n>` for a real title is still only a
    // numeric key, so both are treated as opaque. Resolving a real Steam
    // title's human name from its appmanifest is a later improvement layered
    // on top of this, not a replacement for it.
    static const QRegularExpression steamKey(
        QStringLiteral("^steam_app_[0-9]+$"));
    if (steamKey.match(lowered).hasMatch()) {
        return true;
    }
    return isWineShimClass(lowered);
}

QString executableFromCommandLine(const QByteArray &cmdline)
{
    if (cmdline.isEmpty() || cmdline.size() > maximumCommandLineBytes) {
        return {};
    }
    const QList<QByteArray> arguments = cmdline.split('\0');
    QString firstArgument;
    QString lastWindowsExecutable;
    for (const QByteArray &argument : arguments) {
        if (argument.isEmpty()) {
            continue;
        }
        const QString candidate = basenameOf(
            QString::fromLocal8Bit(argument).trimmed());
        if (!isAcceptableExecutable(candidate)) {
            continue;
        }
        if (firstArgument.isEmpty()) {
            firstArgument = candidate;
        }
        if (candidate.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
            lastWindowsExecutable = candidate;
        }
    }
    // AGENT-GUARD: prefer the LAST `.exe`. A Wine/Proton command line begins
    // with launcher shims (umu-run, proton, wine) and names the real program
    // afterwards, so argv[0] is exactly the wrong answer there.
    return lastWindowsExecutable.isEmpty() ? firstArgument
                                           : lastWindowsExecutable;
}

QString resolveApplicationId(const QString &desktopFileName,
                             const QString &resourceClass,
                             const QString &clientExecutable)
{
    if (!desktopFileName.isEmpty()) {
        return desktopFileName;
    }
    if (!isOpaqueLauncherClass(resourceClass)) {
        return resourceClass;
    }
    if (isAcceptableExecutable(clientExecutable)) {
        return clientExecutable;
    }
    return resourceClass;
}

QString resolveApplicationName(const QString &resourceClass,
                               const QString &clientExecutable,
                               const QString &applicationId)
{
    if (!isOpaqueLauncherClass(resourceClass)) {
        return resourceClass;
    }
    if (isAcceptableExecutable(clientExecutable)) {
        return clientExecutable;
    }
    return resourceClass.isEmpty() ? applicationId : resourceClass;
}

} // namespace QindaQt::Compositor
