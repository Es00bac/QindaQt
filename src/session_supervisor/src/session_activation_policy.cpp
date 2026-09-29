// SPDX-License-Identifier: GPL-3.0-or-later
#include "session_activation_policy.h"

#include "qindaqt/compositor_names/compositor_names.h"

#include <QFile>
#include <QFileInfo>

namespace QindaQt::SessionSupervisor {
SessionActivationScope activationScopeForCompositor(
    const QString &executable, const QStringList &arguments, const QString &processName)
{
    // Only QindaQt's own compositor (ADR-0291) starts the physical session.
    const QString expected = QString(CompositorNames::executable);
    const bool hasIdentity = !executable.isEmpty()
        ? QFileInfo(executable).fileName() == expected
        : processName == expected && !arguments.isEmpty()
            && QFileInfo(arguments.constFirst()).fileName() == expected;
    if (!hasIdentity)
        return SessionActivationScope::Private;
    bool physical = false;
    for (const QString &argument : arguments.mid(1)) {
        if (argument == QStringLiteral("--exit-with-session")
            || argument.startsWith(QStringLiteral("--exit-with-session="))
            || argument == QStringLiteral("--")) break;
        if (argument == QStringLiteral("--virtual")
            || argument == QStringLiteral("--x11")
            || argument == QStringLiteral("--wayland")
            || argument == QStringLiteral("--wayland-display")
            || argument == QStringLiteral("--x11-display")
            || argument.startsWith(QStringLiteral("--wayland-display="))
            || argument.startsWith(QStringLiteral("--x11-display="))
            || argument == QStringLiteral("--windowed")
            || argument.startsWith(QStringLiteral("--virtual="))
            || argument.startsWith(QStringLiteral("--x11="))
            || argument.startsWith(QStringLiteral("--wayland="))
            || argument.startsWith(QStringLiteral("--windowed=")))
            return SessionActivationScope::Private;
        if (argument == QStringLiteral("--drm")) physical = true;
    }
    return physical ? SessionActivationScope::PhysicalDesktop : SessionActivationScope::Private;
}

SessionActivationScope witnessedSessionActivationScope(const qint64 compositorPid)
{
    if (compositorPid <= 1) return SessionActivationScope::Private;
    const QString root = QStringLiteral("/proc/%1/").arg(compositorPid);
    const QString executable = QFileInfo(root + QStringLiteral("exe")).symLinkTarget();
    QFile commandLine(root + QStringLiteral("cmdline"));
    if (!commandLine.open(QIODevice::ReadOnly))
        return SessionActivationScope::Private;
    constexpr qint64 maximumBytes = 64 * 1024;
    const QByteArray bytes = commandLine.read(maximumBytes + 1);
    if (bytes.isEmpty() || bytes.size() > maximumBytes || !bytes.endsWith('\0'))
        return SessionActivationScope::Private;
    QStringList arguments;
    for (const QByteArray &argument : bytes.chopped(1).split('\0'))
        arguments.append(QString::fromLocal8Bit(argument));
    QString processName;
    if (executable.isEmpty()) {
        // AGENT-NOTE: cap_sys_nice makes installed KWin nondumpable, hiding
        // /proc/PID/exe even from its same-UID child. comm and argv[0] remain
        // readable; require both rather than disabling physical login setup.
        QFile comm(root + QStringLiteral("comm"));
        if (!comm.open(QIODevice::ReadOnly)) return SessionActivationScope::Private;
        const QByteArray name = comm.read(17);
        // Linux comm is at most 15 bytes plus one newline. Never accept a
        // partial/truncated identity or arbitrary whitespace normalization.
        if (name.isEmpty() || name.size() > 16 || !name.endsWith('\n'))
            return SessionActivationScope::Private;
        processName = QString::fromLocal8Bit(name.chopped(1));
    }
    return activationScopeForCompositor(executable, arguments, processName);
}
}
