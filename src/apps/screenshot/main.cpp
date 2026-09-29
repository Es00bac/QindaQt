// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/screenshot_app.h"
#include "command_line.h"

#include <QGuiApplication>
#include <QStringList>

#include <cstdio>

// Exit status: 0 done, 1 failed, 2 bad arguments, 3 cancelled by the user.
int main(int argc, char **argv)
{
    // AGENT-NOTE: arguments are parsed before any platform plugin loads, so
    // --help, --version and a usage error work in scripts with no display.
    QStringList arguments;
    for (int index = 0; index < argc; ++index)
        arguments.append(QString::fromLocal8Bit(argv[index]));
    const QindaQt::Screenshot::Invocation invocation =
        QindaQt::Screenshot::parseInvocation(arguments);
    if (invocation.handled) {
        std::fputs(qPrintable(invocation.handledText), stdout);
        return 0;
    }
    if (!invocation.error.isEmpty()) {
        std::fprintf(stderr, "qindaqt-screenshot: %s\n", qPrintable(invocation.error));
        return 2;
    }

    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-screenshot"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("Screenshot"));
    QCoreApplication::setOrganizationName(QStringLiteral("QindaQt"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    // AGENT-CONTRACT: KWin authorizes ScreenShot2 by matching this process's
    // executable to the installed org.qindaqt.Screenshot desktop entry
    // (ADR-0289); the desktop file name also groups its notifications.
    QGuiApplication::setDesktopFileName(QStringLiteral("org.qindaqt.Screenshot"));
    // Windows come and go during a capture; the app decides when to exit.
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QindaQt::Screenshot::ScreenshotApp app(invocation);
    if (!app.start())
        return app.exitCode() == 0 ? 1 : app.exitCode();
    return application.exec();
}
