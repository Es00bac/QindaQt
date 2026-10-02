// SPDX-License-Identifier: GPL-3.0-or-later
#include "misc_dialog.h"
#include "print_job.h"
#include <qindaqt/platform/foreign_parent/foreign_parent.h>
#include <QApplication>
#include <QElapsedTimer>
#include <optional>
#include <QJsonDocument>
#include <QPrintDialog>
#include <QPushButton>
#include <QSocketNotifier>
#include <QTimer>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
namespace {
std::optional<QJsonObject> initialFrame() {
    QByteArray input; QElapsedTimer timer; timer.start();
    while (input.size() <= 8388608) {
        const auto remaining = 2000 - timer.elapsed(); pollfd fd{STDIN_FILENO, POLLIN, 0};
        if (remaining <= 0 || poll(&fd, 1, static_cast<int>(remaining)) <= 0) return {};
        char buffer[65536]; const auto count = read(STDIN_FILENO, buffer, sizeof(buffer)); if (count <= 0) return {};
        input.append(buffer, static_cast<qsizetype>(count));
        const auto boundary = input.indexOf('\n');
        if (boundary >= 0) {
            if (boundary != input.size() - 1) return {};
            QJsonParseError error; const auto doc = QJsonDocument::fromJson(input.left(boundary), &error);
            return error.error == QJsonParseError::NoError && doc.isObject() ? std::optional(doc.object()) : std::nullopt;
        }
    }
    return {};
}
void reply(QApplication &app, int response, const QJsonObject &results) {
    const auto bytes = QJsonDocument(QJsonObject{{"response", response}, {"results", results}}).toJson(QJsonDocument::Compact);
    qsizetype offset = 0; while (offset < bytes.size()) { const auto n = write(STDOUT_FILENO, bytes.constData() + offset, static_cast<size_t>(bytes.size() - offset)); if (n <= 0) { app.exit(2); return; } offset += n; }
    app.exit(0);
}
}
int main(int argc, char **argv) {
    rlimit cores{0, 0}; if (setrlimit(RLIMIT_CORE, &cores) != 0 || prctl(PR_SET_DUMPABLE, 0) != 0) return 2;
    signal(SIGPIPE, SIG_IGN); if (qEnvironmentVariable("WAYLAND_SOCKET").isEmpty()) return 2;
    const auto frame = initialFrame(); if (!frame) return 2;
    const int suppliedPrintFd = frame->value("print_fd").toInt(-1);
    if (suppliedPrintFd >= 0 && fcntl(suppliedPrintFd, F_SETFD, FD_CLOEXEC) < 0) return 2;
    QApplication app(argc, argv); using namespace QindaQt::Services::Portal;
    const auto kind = frame->value("type").toString();
    std::unique_ptr<QDialog> dialog; QPrinter printer; bool ready = false; bool failed = false;
    QindaQt::Platform::ForeignParent::ForeignParent parent;
    const bool printing = kind == "prepare-print" || kind == "print";
    if (printing) {
        if (frame->value("configuration").isObject()) loadPrintConfiguration(&printer, frame->value("configuration").toObject());
        else loadPrintSettings(&printer, frame->value("settings").toObject().toVariantMap(), frame->value("page-setup").toObject().toVariantMap());
        printer.setDocName(frame->value("title").toString());
        auto print = std::make_unique<QPrintDialog>(&printer); print->setWindowTitle(frame->value("title").toString());
        print->setWindowModality(frame->value("modal").toBool(true) ? Qt::ApplicationModal : Qt::NonModal); print->setEnabled(false);
        const auto label = frame->value("accept_label").toString();
        if (!label.isEmpty()) for (auto *button : print->findChildren<QPushButton *>()) if (button->text().contains("Print")) button->setText(label);
        dialog = std::move(print);
    } else dialog = std::make_unique<MiscDialog>(*frame);
    auto fail = [&] { failed = true; dialog->done(QDialog::Rejected); };
    QObject::connect(dialog.get(), &QDialog::finished, &app, [&](int code) {
        if (!printing) { auto *misc = static_cast<MiscDialog *>(dialog.get()); reply(app, failed ? 2 : misc->response(), failed ? QJsonObject{} : misc->results()); return; }
        if (!ready || failed || code != QDialog::Accepted) { reply(app, failed || !ready ? 2 : 1, {}); return; }
        if (kind == "prepare-print") reply(app, 0, savePrintSettings(&printer));
        else { const int fd = frame->value("print_fd").toInt(-1); const bool ok = submitPrint(printer, fd, runPrintCommand); if (fd >= 0) close(fd); reply(app, ok ? 0 : 2, {}); }
    });
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::ready, &app, [&] {
        ready = true; dialog->setEnabled(true);
        if (!printing) static_cast<MiscDialog *>(dialog.get())->markReady();
        else if (kind == "print" && frame->value("configuration").isObject()) dialog->done(QDialog::Accepted);
    });
    QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::lost, &app, fail);
    dialog->show(); if (!dialog->windowHandle()) return 2; parent.attach(*dialog->windowHandle(), frame->value("parent").toString());
    QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read, &app);
    QObject::connect(&input, &QSocketNotifier::activated, &app, [&] { char byte; if (read(STDIN_FILENO, &byte, 1) <= 0) { input.setEnabled(false); fail(); } else fail(); });
    QTimer::singleShot(60000, &app, fail); return app.exec();
}
