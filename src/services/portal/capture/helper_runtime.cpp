// SPDX-License-Identifier: GPL-3.0-or-later
#include "helper_runtime.h"
#include "authority/channel.h"
#include "capture_dialog.h"
#include <qindaqt/platform/foreign_parent/foreign_parent.h>
#include <QApplication>
#include <QJsonDocument>
#include <QDir>
#include <QFile>
#include <QSocketNotifier>
#include <QTimer>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
#ifdef QINDAQT_CAPTURE_ACTUAL_INPUT_TEST
// Linked solely into a non-installable actual-input helper. Production's
// strict frame parser never accepts these test-only presentation directives.
bool captureTestFrame(QJsonObject &, qint64 compositorPid);
void captureTestControlEvent(const char *);
#endif
namespace {
bool pipeEnd(int fd, int mode) {
    struct stat info{}; const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && (flags & O_ACCMODE) == mode && !fstat(fd, &info) && S_ISFIFO(info.st_mode)
        && !fcntl(fd, F_SETFL, flags | O_NONBLOCK) && !fcntl(fd, F_SETFD, FD_CLOEXEC);
}
}
class HelperRuntime::Private {
public:
    HelperRuntime &q; QDBusConnection bus; CaptureAuthority::Channel control;
    std::unique_ptr<NativeCaptureAdmission> admission;
    std::unique_ptr<CompositorCapture::KWinCapturePort> pixels;
    std::unique_ptr<CompositorCapture::WaylandScreenCast> stream;
    std::unique_ptr<CaptureDialog> dialog;
    QindaQt::Platform::ForeignParent::ForeignParent parent;
    QSocketNotifier *requestReader = nullptr, *resultWriter = nullptr;
    QTimer inputDeadline, consentDeadline, outputDeadline, retentionDeadline;
    QByteArray input, output; QString directory; std::optional<CaptureRequest> request;
    bool stopping = false, granted = false, written = false, resultPending = false, imageCreated = false;
    Private(HelperRuntime &runtime, QDBusConnection connection)
        : q(runtime), bus(connection), control(CaptureAuthority::Wire::ControlFd, CaptureAuthority::Role::Helper, connection) {
        for (auto *timer : {&inputDeadline, &consentDeadline, &outputDeadline, &retentionDeadline}) {
            timer->setSingleShot(true); QObject::connect(timer, &QTimer::timeout, &q, [this] { retire(); });
        }
    }
    bool live() const { return !stopping && control.live() && admission && admission->admitted(); }
    void retire() {
        if (stopping) return;
        stopping = true; granted = false; output.clear();
        if (control.live()) control.send(CaptureAuthority::Wire::Message::RevokeJob, control.job());
        for (auto *timer : {&inputDeadline, &consentDeadline, &outputDeadline, &retentionDeadline}) timer->stop();
        if (requestReader) requestReader->setEnabled(false);
        if (resultWriter) resultWriter->setEnabled(false);
        if (pixels) pixels->cancel();
        if (stream) stream->stop();
        if (dialog) dialog->fail();
        // Broker exit closes its request pipe; this surviving helper also
        // withdraws only the image it actually created for that trusted job.
        if (imageCreated) QFile::remove(QDir(directory).filePath("screenshot.png"));
        ::close(CaptureAuthority::Wire::ResultWriteFd); control.stop(); QCoreApplication::quit();
    }
    void receive(CaptureAuthority::ReceivedPacket packet) {
        using Message = CaptureAuthority::Wire::Message;
#ifdef QINDAQT_CAPTURE_ACTUAL_INPUT_TEST
        if (packet.packet.message == Message::Hello) captureTestControlEvent("Ready");
        else if (packet.packet.message == Message::CaptureReady) captureTestControlEvent("CaptureReady");
        else if (packet.packet.message == Message::JobRevoked) captureTestControlEvent("JobRevoked");
        else if (packet.packet.message == Message::Error) captureTestControlEvent("Error");
#endif
        if (packet.packet.message == Message::Hello) { inputDeadline.start(2000); readRequest(); return; }
        if (packet.packet.message == Message::CaptureReady && request && dialog && live() && !granted) {
            granted = true; dialog->captureReady(); return;
        }
        retire();
    }
    void readRequest() {
        if (stopping || !control.live()) return;
        char bytes[2048];
        while (true) {
            const auto count = ::read(CaptureAuthority::Wire::RequestReadFd, bytes, sizeof(bytes));
            if (count > 0) {
                if (request || input.size()+count > 16384) { retire(); return; }
                input.append(bytes, count); continue;
            }
            if (count < 0 && errno == EINTR) continue;
            if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) { retire(); return; }
            break;
        }
        const auto end = input.indexOf('\n'); if (end < 0) return;
        if (end != input.size()-1 || request) { retire(); return; }
        QJsonParseError error; auto frame = QJsonDocument::fromJson(input.left(end), &error).object(); input.clear();
#ifdef QINDAQT_CAPTURE_ACTUAL_INPUT_TEST
        if (!captureTestFrame(frame, control.peerPid())) { retire(); return; }
#endif
        request = captureRequestFromFrame(frame);
        if (error.error != QJsonParseError::NoError || !request || frame.value("owner").toString() != control.owner()) { retire(); return; }
        inputDeadline.stop(); directory = frame.value("directory").toString();
        admission = std::make_unique<NativeCaptureAdmission>(bus, control.owner(), CaptureAuthority::Wire::ControlFd);
        pixels = std::make_unique<CompositorCapture::KWinCapturePort>(bus);
        stream = std::make_unique<CompositorCapture::WaylandScreenCast>(CaptureAuthority::Wire::CaptureWaylandFd,
            [this] { return !stopping && control.live() && admission->lineageLive(); }, [this] { return granted && live(); });
        dialog = std::make_unique<CaptureDialog>(*request, frame.value("directory").toString(), *admission, *pixels, *stream);
        QObject::connect(admission.get(), &NativeCaptureAdmission::lost, &q, [this] { retire(); });
        QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::ready, &q, [this] {
            if (!control.send(CaptureAuthority::Wire::Message::ParentReady, control.job())) { retire(); return; }
            dialog->parentReady();
        });
        QObject::connect(&parent, &QindaQt::Platform::ForeignParent::ForeignParent::lost, &q, [this] { retire(); });
        QObject::connect(dialog.get(), &CaptureDialog::consented, &q, [this] {
            consentDeadline.stop();
            if (!live() || !control.send(CaptureAuthority::Wire::Message::ConsentGranted, control.job())) retire();
        });
        QObject::connect(dialog.get(), &CaptureDialog::result, &q, [this](RequestResponse response, const QJsonObject &results) { result(response, results); });
        QObject::connect(dialog.get(), &QDialog::finished, &q, [this] { if (request->kind == CaptureKind::Stream && granted && written) retire(); });
        dialog->show(); if (!dialog->windowHandle()) { retire(); return; }
        parent.attach(*dialog->windowHandle(), request->parent); consentDeadline.start(60000);
    }
    void result(RequestResponse response, const QJsonObject &results) {
        if (response == RequestResponse::Success && request && request->kind == CaptureKind::Screenshot)
            imageCreated = captureResults(request->kind, results, directory).has_value();
        if (stopping || !control.live() || written || resultPending) { retire(); return; }
        if (response == RequestResponse::Success && (!granted || !live())) { retire(); return; }
        consentDeadline.stop(); resultPending = true;
        output = QJsonDocument(QJsonObject{{"response", static_cast<int>(response)}, {"results", results}}).toJson(QJsonDocument::Compact)+'\n';
        if (output.size() > 16384) { retire(); return; }
        outputDeadline.start(2000); resultWriter->setEnabled(true); writeResult();
    }
    void writeResult() {
        if (stopping || !resultPending) return;
        if (!control.live()) { retire(); return; }
        while (!output.isEmpty()) {
            const auto count = ::write(CaptureAuthority::Wire::ResultWriteFd, output.constData(), static_cast<size_t>(output.size()));
            if (count > 0) { output.remove(0, count); continue; }
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
            retire(); return;
        }
        outputDeadline.stop(); resultWriter->setEnabled(false); resultPending = false; written = true;
        // Do not exit on dialog completion: HUP would revoke before broker
        // publication. Hidden Screenshot/PickColor wait for broker retirement;
        // a streaming dialog remains visible with actual Stop action.
        if (request->kind != CaptureKind::Stream) retentionDeadline.start(300000);
    }
    bool start() {
        if (!pipeEnd(CaptureAuthority::Wire::RequestReadFd, O_RDONLY) || !pipeEnd(CaptureAuthority::Wire::ResultWriteFd, O_WRONLY)) return false;
        requestReader = new QSocketNotifier(CaptureAuthority::Wire::RequestReadFd, QSocketNotifier::Read, &q);
        resultWriter = new QSocketNotifier(CaptureAuthority::Wire::ResultWriteFd, QSocketNotifier::Write, &q); resultWriter->setEnabled(false);
        QObject::connect(requestReader, &QSocketNotifier::activated, &q, [this] { readRequest(); });
        QObject::connect(resultWriter, &QSocketNotifier::activated, &q, [this] { writeResult(); });
        return control.start([this](CaptureAuthority::ReceivedPacket &&packet) { receive(std::move(packet)); }, [this] {
#ifdef QINDAQT_CAPTURE_ACTUAL_INPUT_TEST
            captureTestControlEvent("lost");
#endif
            retire();
        });
    }
};
HelperRuntime::HelperRuntime(QDBusConnection bus) : d(std::make_unique<Private>(*this, std::move(bus))) {}
HelperRuntime::~HelperRuntime() { d->retire(); ::close(CaptureAuthority::Wire::RequestReadFd); }
bool HelperRuntime::start() { return d->start(); }
}
