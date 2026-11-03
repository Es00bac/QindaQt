// SPDX-License-Identifier: GPL-3.0-or-later
// qindaqt-link-mirror: one phone app in one desktop window, streamed over ADB.
// Contract: docs/wiki/reference/qindalink-mirror-v1.md. Architecture: docs/wiki/architecture/qindalink.md.

#include "evidence_runner.h"
#include "ffmpeg_decoder.h"
#include "mirror_controller.h"
#include "mirror_options.h"
#include "mirror_window.h"

#include <QGuiApplication>
#include <QSocketNotifier>

#include <QTimer>

#include <csignal>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>

namespace {

int g_signalPipe[2] = {-1, -1};

void onTerminationSignal(int)
{
    const char byte = 1;
    // Async-signal-safe hand-off to the event loop; a full pipe already means "quit".
    [[maybe_unused]] const ssize_t written = ::write(g_signalPipe[0], &byte, 1);
}

// AGENT-GUARD: Ctrl+C or a session kill must still run MirrorController::finish(), which
// removes the adb forward and lets the phone server release its virtual display.
void installTerminationHandler(QObject *context, const std::function<void()> &onSignal)
{
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, g_signalPipe) != 0) {
        return;
    }
    auto *notifier = new QSocketNotifier(g_signalPipe[1], QSocketNotifier::Read, context);
    QObject::connect(notifier, &QSocketNotifier::activated, context, [notifier, onSignal] {
        char byte = 0;
        [[maybe_unused]] const ssize_t got = ::read(g_signalPipe[1], &byte, 1);
        notifier->setEnabled(false);
        onSignal();
    });
    struct sigaction action {};
    action.sa_handler = &onTerminationSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGHUP, &action, nullptr);
}

bool wantsHeadless(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--grab") == 0 || std::strcmp(argv[i], "--probe") == 0
            || std::strcmp(argv[i], "--list") == 0 || std::strcmp(argv[i], "--list-json") == 0 || std::strncmp(argv[i], "--grab=", 7) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    using namespace qindaqt::link_mirror;
    // Evidence and inventory runs must never map a window on the user's desktop.
    if (wantsHeadless(argc, argv) && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("qindaqt-link-mirror"));
    QGuiApplication::setQuitOnLastWindowClosed(false);

    QString error;
    const auto parsed = parseMirrorOptions(QGuiApplication::arguments(), &error);
    if (!parsed) {
        std::fprintf(stderr, "qindaqt-link-mirror: %s\n", qPrintable(error));
        return 2;
    }
    const MirrorOptions options = *parsed;
    if (!options.list) {
        // The Wayland app id; the task list groups and labels mirror windows by it.
        QGuiApplication::setDesktopFileName(mirrorAppId(options.packageName));
    }
    quietFfmpegLogging(options.verbose);

    ViewportMapper mapper;
    MirrorWindow window(mapper);
    window.setTitle(options.packageName);
    MirrorController controller(options, window, mapper);
    QObject::connect(&controller, &MirrorController::finished, &app, [](int exitCode) {
        QCoreApplication::exit(exitCode);
    });
    installTerminationHandler(&app, [&controller] { controller.finish(0); });

    // AGENT-GUARD: start from inside the event loop. start() can fail synchronously, and a
    // QCoreApplication::exit() issued before exec() is lost, which would hang the process.
    std::unique_ptr<EvidenceRunner> evidence;
    if (options.headless()) {
        mapper.setViewSize(options.size);
        evidence = std::make_unique<EvidenceRunner>(options, controller, window, mapper);
        QTimer::singleShot(0, &controller, [&controller, &options] { controller.start(options.size, 1.0); });
    } else {
        window.resize(options.size);
        window.create();
        QTimer::singleShot(0, &controller, [&controller, &options, &window] {
            controller.start(options.size, window.devicePixelRatio());
            window.show();
        });
    }
    return app.exec();
}
