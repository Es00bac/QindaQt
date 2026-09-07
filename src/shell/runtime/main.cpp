// SPDX-License-Identifier: GPL-3.0-or-later
#include "shellruntimeapplication.h"

#include <QCoreApplication>
#include <QGuiApplication>

int main(int argc, char *argv[])
{
    // AGENT-NOTE: ADR-0104 mitigates the observed Qt Wayland beginFrame crash
    // by serializing shell rendering with window lifetime events. Keep an
    // explicit override available for diagnosis; this is not a root-cause fix.
    if (qEnvironmentVariableIsEmpty("QSG_RENDER_LOOP")) {
        qputenv("QSG_RENDER_LOOP", "basic");
    }
    QGuiApplication application(argc, argv);
    // AGENT-GUARD: KWin dismisses layer surfaces when an output disappears.
    // The shell must survive a transient zero-window phase and reconcile the
    // remaining output inventory instead of ending the desktop session.
    application.setQuitOnLastWindowClosed(false);
    QCoreApplication::setApplicationName(QStringLiteral("qindaqt-shell"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("QindaQt Shell"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QINDAQT_VERSION));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qindaqt.org"));

    QindaQt::Shell::ShellRuntimeApplication shell(application);
    return shell.run();
}
