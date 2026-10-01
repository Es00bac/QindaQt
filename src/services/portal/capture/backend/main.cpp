// SPDX-License-Identifier: GPL-3.0-or-later
#include "../authority/packet.h"
#include "authority_capture.h"
#include <qindaqt/services/portal/screenshot_adaptor.h>
#include <qindaqt/services/portal/screencast_adaptor.h>
#include <QCoreApplication>
#include <QStandardPaths>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc, char **argv) {
    using namespace QindaQt::Services::Portal;
    // AGENT-GUARD: this fixed capture-only executable is compositor-launched;
    // it has no general backend/Session1 attachment or standalone activation.
    // Protected process state precedes Qt, bus and inherited-channel parsing.
    rlimit cores{0, 0};
    if (argc != 1 || setrlimit(RLIMIT_CORE, &cores) || prctl(PR_SET_DUMPABLE, 0) || prctl(PR_GET_DUMPABLE) != 0) return 2;
    signal(SIGPIPE, SIG_IGN); umask(0077);
    QCoreApplication app(argc, argv); auto bus = QDBusConnection::sessionBus();
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    struct stat directory{}; const auto path = runtime.toUtf8();
    if (runtime.isEmpty() || lstat(path.constData(), &directory) || !S_ISDIR(directory.st_mode)
        || directory.st_uid != geteuid() || (directory.st_mode & 0077) || !bus.isConnected()
        || !bus.registerService("org.freedesktop.impl.portal.desktop.qindaqt.capture")) return 2;
    RequestRegistry requests(bus);
    AuthorityCapture capture(requests, bus, runtime, CaptureAuthority::Wire::ControlFd);
    if (!capture.available()) return 2;
    QObject host; ScreenshotAdaptor screenshot(host, requests, capture); ScreenCastAdaptor screencast(host, requests, capture, bus);
    if (!bus.registerObject("/org/freedesktop/portal/desktop", &host, QDBusConnection::ExportAdaptors)) return 2;
    QObject::connect(&capture, &CaptureUI::authorityLost, &app, [&] { requests.retireAll(); app.quit(); });
    return app.exec();
}
