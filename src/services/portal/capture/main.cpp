// SPDX-License-Identifier: GPL-3.0-or-later
#include "authority/packet.h"
#include "helper_runtime.h"
#include <QApplication>
#include <QDBusConnection>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc, char **argv) {
    using namespace QindaQt::Services::Portal;
    rlimit cores{0, 0};
    if (argc != 1 || setrlimit(RLIMIT_CORE, &cores) || prctl(PR_SET_DUMPABLE, 0) || prctl(PR_GET_DUMPABLE) != 0) return 2;
    signal(SIGPIPE, SIG_IGN); umask(0077);
    // Only the compositor-created ordinary GUI fd is supplied to Qt. The
    // separate restricted fd5 is never used for Qt/foreign-parent globals.
    qunsetenv("WAYLAND_DISPLAY"); qunsetenv("DISPLAY");
    qputenv("WAYLAND_SOCKET", QByteArray::number(CaptureAuthority::Wire::GuiWaylandFd));
    qputenv("QT_QPA_PLATFORM", "wayland"); qputenv("QT_STYLE_OVERRIDE", "Fusion");
    QApplication app(argc, argv); app.setQuitOnLastWindowClosed(false);
    const auto bus = QDBusConnection::sessionBus(); if (!bus.isConnected()) return 2;
    HelperRuntime runtime(bus); if (!runtime.start()) return 2;
    return app.exec();
}
