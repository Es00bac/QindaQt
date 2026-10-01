// SPDX-License-Identifier: GPL-3.0-or-later
#include "file_chooser_dialog.h"
#include "app_chooser_dialog.h"
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
std::optional<QJsonObject> initialFrame() {
    QByteArray input; QElapsedTimer timer; timer.start();
    while (input.size() <= 131072) {
        const auto remaining = 2000 - timer.elapsed(); pollfd fd{STDIN_FILENO, POLLIN, 0};
        if (remaining <= 0 || poll(&fd, 1, static_cast<int>(remaining)) <= 0) return {};
        char byte = 0; if (read(STDIN_FILENO, &byte, 1) != 1) return {};
        if (byte == '\n') { QJsonParseError error; const auto doc = QJsonDocument::fromJson(input, &error); return error.error == QJsonParseError::NoError && doc.isObject() ? std::optional(doc.object()) : std::nullopt; }
        input.append(byte);
    }
    return {};
}
}
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores) != 0 || prctl(PR_SET_DUMPABLE, 0) != 0) return 2;
    signal(SIGPIPE, SIG_IGN);
    if (qEnvironmentVariable("WAYLAND_SOCKET").isEmpty()) return 2;
    const auto frame = initialFrame(); if (!frame) return 2;
    QApplication app(argc, argv);
    using namespace QindaQt::Services::Portal;
    std::unique_ptr<ChooserDialog> dialog; QString parentWindow;
    if (frame->value("type") == QJsonValue("file")) {
        const auto request = fileChooserFromFrame(*frame); if (!request) return 2;
        parentWindow = request->question.parentWindow; dialog = std::make_unique<FileChooserDialog>(*request);
    } else {
        const auto request = appChooserFromFrame(*frame); if (!request) return 2;
        parentWindow = request->question.parentWindow; dialog = std::make_unique<AppChooserDialog>(*request);
    }
    QObject::connect(dialog.get(), &QDialog::finished, &app, [&] {
        const auto bytes = QJsonDocument(QJsonObject{{"response", static_cast<int>(dialog->response())}, {"results", dialog->results()}}).toJson(QJsonDocument::Compact);
        const auto written = write(STDOUT_FILENO, bytes.constData(), static_cast<size_t>(bytes.size()));
        app.exit(written == bytes.size() ? 0 : 2);
    });
    QindaQt::Platform::ForeignParent::ForeignParent parent;
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::ready, dialog.get(), &ChooserDialog::markReady);
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::lost, dialog.get(), &ChooserDialog::fail);
    dialog->show(); if (!dialog->windowHandle()) return 2; parent.attach(*dialog->windowHandle(), parentWindow);
    QByteArray updates; int frames = 0;
    QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read, &app);
    QObject::connect(&notifier, &QSocketNotifier::activated, &app, [&] {
        char buffer[4096]; const auto count = read(STDIN_FILENO, buffer, sizeof(buffer));
        if (count <= 0) { dialog->fail(); notifier.setEnabled(false); return; }
        updates.append(buffer, static_cast<qsizetype>(count));
        if (updates.size() > 131072) { dialog->fail(); return; }
        while (updates.contains('\n')) {
            const auto boundary = updates.indexOf('\n'); const auto bytes = updates.left(boundary); updates.remove(0, boundary + 1);
            QJsonParseError error; const auto object = QJsonDocument::fromJson(bytes, &error).object();
            if (++frames > 128 || error.error != QJsonParseError::NoError || object.size() != 2
                || object.value("type") != QJsonValue("update") || !object.value("candidates").isArray()
                || frame->value("type") != QJsonValue("app")) { dialog->fail(); return; }
            dialog->updateCandidates(object.value("candidates").toArray());
        }
    });
    QTimer::singleShot(60000, dialog.get(), &ChooserDialog::fail);
    return app.exec();
}
