// SPDX-License-Identifier: GPL-3.0-or-later
#include "capture_dialog.h"
#include <qindaqt/platform/foreign_parent/foreign_parent.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QTimer>
#include <poll.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
namespace {
std::optional<QJsonObject> frame() {
    QByteArray input; QElapsedTimer clock; clock.start();
    while (input.size() <= 16384) {
        const auto remaining = 2000-clock.elapsed(); pollfd fd{STDIN_FILENO, POLLIN, 0};
        if (remaining <= 0 || poll(&fd, 1, static_cast<int>(remaining)) <= 0) return {};
        char byte = 0; if (read(STDIN_FILENO, &byte, 1) != 1) return {};
        if (byte == '\n') { QJsonParseError error; const auto doc = QJsonDocument::fromJson(input, &error); return error.error == QJsonParseError::NoError && doc.isObject() ? std::optional(doc.object()) : std::nullopt; }
        input.append(byte);
    }
    return {};
}
}
int main(int argc, char **argv) {
    using namespace QindaQt::Services::Portal;
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores) != 0 || prctl(PR_SET_DUMPABLE, 0) != 0) return 2; signal(SIGPIPE, SIG_IGN);
    bool valid = false; const int socket = qEnvironmentVariableIntValue("QINDAQT_CAPTURE_SOCKET", &valid);
    if (!valid || socket < 0 || qEnvironmentVariable("WAYLAND_SOCKET").isEmpty()) return 2;
    const auto input = frame(); if (!input) return 2; const auto request = captureRequestFromFrame(*input); if (!request) return 2;
    QApplication app(argc, argv); const auto bus = QDBusConnection::sessionBus();
    NativeCaptureAdmission admission(bus, input->value("owner").toString(), socket);
    QindaQt::CompositorCapture::KWinCapturePort pixels(bus);
    QindaQt::CompositorCapture::WaylandScreenCast stream(socket, [&admission] { return admission.lineageLive(); }, [&admission] { return admission.admitted(); });
    CaptureDialog dialog(*request, input->value("directory").toString(), admission, pixels, stream);
    QObject::connect(&dialog, &CaptureDialog::result, &app, [](RequestResponse response, const QJsonObject &results) {
        const auto output = QJsonDocument(QJsonObject{{"response", static_cast<int>(response)}, {"results", results}}).toJson(QJsonDocument::Compact) + '\n';
        if (write(STDOUT_FILENO, output.constData(), static_cast<size_t>(output.size())) != output.size()) QCoreApplication::exit(2);
    });
    QObject::connect(&dialog, &QDialog::finished, &app, [&app] { app.exit(0); });
    QindaQt::Platform::ForeignParent::ForeignParent parent;
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::ready, &dialog, &CaptureDialog::parentReady);
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::lost, &dialog, &CaptureDialog::fail);
    dialog.show(); if (!dialog.windowHandle()) return 2; parent.attach(*dialog.windowHandle(), request->parent);
    QSocketNotifier inputLifetime(STDIN_FILENO, QSocketNotifier::Read, &app);
    QObject::connect(&inputLifetime, &QSocketNotifier::activated, &dialog, [&] { char byte = 0; static_cast<void>(read(STDIN_FILENO, &byte, 1)); inputLifetime.setEnabled(false); dialog.fail(); });
    QTimer::singleShot(60000, &dialog, [&] { if (request->kind != CaptureKind::Stream) dialog.fail(); });
    return app.exec();
}
