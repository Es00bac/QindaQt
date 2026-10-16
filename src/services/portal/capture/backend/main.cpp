// SPDX-License-Identifier: GPL-3.0-or-later
#include "../authority/packet.h"
#include "authority_capture.h"
#include <qindaqt/services/portal/screenshot_adaptor.h>
#include <qindaqt/services/portal/process_consent.h>
#include <qindaqt/services/portal/remote_input/remote_desktop_adaptor.h>
#include <qindaqt/services/portal/remote_input/input_capture_adaptor.h>
#include <qindaqt/services/portal/screencast_adaptor.h>
#include <QCoreApplication>
#include <QStandardPaths>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>
namespace {
// AGENT-NOTE: the compositor forwards this process's stderr to the session
// log (qindaqt-kwin CaptureChild) and relaunches it with backoff. Every exit
// says why here: before stability round 2026-10-16 each refusal returned 2
// silently and a dead broker could not be diagnosed (findings item 8).
// Plain stdio: some refusals precede Qt, and the text carries no secrets.
int exitWith(int status, const char *reason, const char *detail = nullptr) {
    std::fprintf(stderr, "qindaqt-portal-capture-backend: exiting with status %d: %s%s%s\n", status, reason,
                 detail ? ": " : "", detail ? detail : "");
    std::fflush(stderr);
    return status;
}
}
int main(int argc, char **argv) {
    using namespace QindaQt::Services::Portal;
    // AGENT-GUARD: this fixed capture/input broker is compositor-launched,
    // never a general resident or standalone activation. Ordinary input consent
    // uses its own attachment; capture grants still require inherited QCC1.
    // Protected process state precedes Qt, bus and inherited-channel parsing.
    rlimit cores{0, 0};
    if (argc != 1) return exitWith(2, "unexpected arguments; only the compositor launches this broker");
    if (setrlimit(RLIMIT_CORE, &cores)) return exitWith(2, "cannot disable core dumps", std::strerror(errno));
    if (prctl(PR_SET_DUMPABLE, 0) || prctl(PR_GET_DUMPABLE) != 0) return exitWith(2, "cannot make the process non-dumpable");
    signal(SIGPIPE, SIG_IGN); umask(0077);
    QCoreApplication app(argc, argv); auto bus = QDBusConnection::sessionBus();
    const auto runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    struct stat directory{}; const auto path = runtime.toUtf8();
    if (runtime.isEmpty()) return exitWith(2, "no XDG runtime directory");
    if (lstat(path.constData(), &directory) || !S_ISDIR(directory.st_mode))
        return exitWith(2, "the XDG runtime directory is missing or not a directory", path.constData());
    if (directory.st_uid != geteuid() || (directory.st_mode & 0077))
        return exitWith(2, "the XDG runtime directory is not private to this user", path.constData());
    if (!bus.isConnected()) return exitWith(2, "the session bus is unavailable");
    RequestRegistry requests(bus);
    AuthorityCapture capture(requests, bus, runtime, CaptureAuthority::Wire::ControlFd);
    if (!capture.available()) return exitWith(2, "the compositor control channel is unavailable");
    // ADR0341: one protected producer/session composition joins screens and
    // input. This additional binding supplies ordinary consent display FDs; it
    // cannot manufacture capture capabilities or replace native QCC1 receipts.
    PortalSessionBinding binding(bus, runtime, PortalBindingTarget::ProtectedCapture);
    ProcessAccessConsent consent(binding, bus, QStringLiteral(QINDAQT_CAPTURE_CONSENT_EXECUTABLE));
    RemoteInput::CompositorEis eis(bus, [&binding] { return binding.compositorOwner(); });
    QObject host;
    RemoteInput::RemoteDesktopAdaptor remote(host, requests, consent, eis, capture, bus);
    RemoteInput::InputCaptureAdaptor inputCapture(host, requests, consent, eis, bus);
    ScreenshotAdaptor screenshot(host, requests, capture);
    ScreenCastAdaptor screencast(host, requests, capture, bus, &remote.screenCastSources());
    QObject::connect(&consent, &AccessConsent::authorityLost, &capture, [&] {
        exitWith(0, "process-access consent authority lost");
        requests.retireAll(); capture.revoke(); app.quit();
    });
    bool published = false, startupFailed = false;
    const auto publish = [&] {
        if (published || !capture.initialized()) return;
        // AGENT-CONTRACT: publish only after this broker consumed both native
        // receipt and reply. Name registration must precede QCC1 Ready because
        // the compositor authenticates its current owner (ADR0324 / WIRE.md).
        // The order of these steps is the protocol; only the logging is new.
        const char *failure = nullptr;
        QByteArray detail;
        if (!binding.start()) {
            failure = "cannot register the protected portal session binding";
        } else if (!bus.registerObject("/org/freedesktop/portal/desktop", &host, QDBusConnection::ExportAdaptors)) {
            failure = "cannot export the portal objects";
        } else if (!capture.initialized()) {
            failure = "native capture admission ended before publication";
        } else if (!bus.registerService("org.freedesktop.impl.portal.desktop.qindaqt.capture")) {
            failure = "cannot own org.freedesktop.impl.portal.desktop.qindaqt.capture";
            detail = bus.lastError().message().toUtf8();
        } else if (!capture.start()) {
            failure = "cannot start the compositor control channel";
        }
        if (failure) {
            bus.unregisterService("org.freedesktop.impl.portal.desktop.qindaqt.capture");
            bus.unregisterObject("/org/freedesktop/portal/desktop");
            exitWith(2, failure, detail.isEmpty() ? nullptr : detail.constData());
            startupFailed = true;
            app.exit(2);
            return;
        }
        published = true;
    };
    QObject::connect(&capture, &AuthorityCapture::initializedChanged, &app,
                     [&](bool initialized) { if (initialized) publish(); });
    // AuthorityCapture logs the specific cause just before this signal.
    QObject::connect(&capture, &CaptureUI::authorityLost, &app, [&] {
        exitWith(0, "compositor capture authority lost");
        requests.retireAll(); app.quit();
    });
    publish();
    if (startupFailed) return 2;
    const int status = app.exec();
    if (status != 0 && !startupFailed) exitWith(status, "event loop ended with an error status");
    return status;
}
