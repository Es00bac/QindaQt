// SPDX-License-Identifier: LGPL-3.0-or-later
#include "authority_capture.h"
#include "../authority/channel.h"
#include "../native_capture_admission.h"
#include <qindaqt/compositor_names/compositor_names.h>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonDocument>
#include <QSocketNotifier>
#include <QTemporaryDir>
#include <QTimer>
#include <cerrno>
#include <limits>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
#ifdef QINDAQT_CAPTURE_ACTUAL_INPUT_TEST
bool augmentCaptureTestFrame(QJsonObject &);
#endif
namespace {
bool pidAlive(int fd) { pollfd event{fd, POLLIN, 0}; return fd >= 0 && poll(&event, 1, 0) == 0; }
QString nativeOwner(const QDBusConnection &bus) {
    if (!bus.interface()) return {};
    const QDBusReply<QString> owner = bus.interface()->serviceOwner(QString(QindaQt::CompositorNames::service));
    return owner.isValid() ? owner.value() : QString{};
}
bool pipeEnd(int fd, int mode) {
    struct stat info{}; const int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && (flags & O_ACCMODE) == mode && !fstat(fd, &info) && S_ISFIFO(info.st_mode)
        && !fcntl(fd, F_SETFL, flags | O_NONBLOCK) && !fcntl(fd, F_SETFD, FD_CLOEXEC);
}
}
class AuthorityCapture::Private {
public:
    struct Job {
        quint64 id = 0; CaptureRequest request; QString frontend;
        std::unique_ptr<QTemporaryDir> directory; QByteArray input, output;
        QSocketNotifier *reader = nullptr, *writer = nullptr;
        int writeFd = -1, readFd = -1, callerPidfd = -1;
        bool started = false, published = false;
        ~Job() {
            for (auto *notifier : {reader, writer}) if (notifier) { notifier->setEnabled(false); notifier->deleteLater(); }
            for (const int fd : {writeFd, readFd, callerPidfd}) if (fd >= 0) ::close(fd);
        }
    };
    AuthorityCapture &q; RequestRegistry &requests; QDBusConnection bus; QString runtime;
    CaptureAuthority::Channel channel; NativeCaptureAdmission admission;
    QHash<RequestToken, std::shared_ptr<Job>> jobs; quint64 next = 0; QTimer lifetime;
    Private(AuthorityCapture &port, RequestRegistry &registry, QDBusConnection connection, QString root, int fd)
        : q(port), requests(registry), bus(connection), runtime(std::move(root)),
          channel(fd, CaptureAuthority::Role::Broker, connection), admission(connection, nativeOwner(connection), fd) {
        QObject::connect(&admission, &NativeCaptureAdmission::lost, &q, [this] { q.revoke(); Q_EMIT q.authorityLost(); });
        lifetime.setInterval(50);
        QObject::connect(&lifetime, &QTimer::timeout, &q, [this] {
            channel.reconcile();
            if (!q.admitted()) { q.revoke(); return; }
            const auto tokens = jobs.keys(); for (const auto token : tokens) if (const auto job = jobs.value(token); job && !live(*job)) { fail(token); retire(token); }
        }); lifetime.start();
    }
    void start() {
        channel.start([this](CaptureAuthority::ReceivedPacket &&packet) { receive(std::move(packet)); }, [this] { q.revoke(); Q_EMIT q.authorityLost(); });
    }
    bool live(const Job &job) const {
        if (!q.admitted() || !pidAlive(job.callerPidfd) || job.frontend != requests.frontendOwner() || !bus.interface()) return false;
        const QDBusReply<QString> caller = bus.interface()->serviceOwner(job.request.caller);
        return caller.isValid() && caller.value() == job.request.caller;
    }
    void fail(RequestToken token) {
        const auto job = jobs.value(token); if (job && job->published) return;
        QTimer::singleShot(0, &q, [this, token] { if (requests.live(token)) Q_EMIT q.completed(token, RequestResponse::Failed, {}); });
    }
    void retire(RequestToken token) {
        const auto job = jobs.take(token); if (!job) return;
        if (channel.live()) channel.send(CaptureAuthority::Wire::Message::RevokeJob, job->id);
        if (!job->request.session.isEmpty()) Q_EMIT q.closed(job->request.session);
    }
    RequestToken tokenFor(quint64 id) const {
        for (auto it = jobs.cbegin(); it != jobs.cend(); ++it) {
            if (it.value()->id == id) return it.key();
        }
        return 0;
    }
    void receive(CaptureAuthority::ReceivedPacket packet) {
        using Message = CaptureAuthority::Wire::Message;
        if (packet.packet.message == Message::Hello) return;
        const auto token = tokenFor(packet.packet.job); const auto job = jobs.value(token);
        if (!job) {
            // A cancelled Start may race JobStarted. Its FDs close with packet;
            // revoke again, never resurrect a removed registry/session token.
            if (packet.packet.job > next) { channel.stop(); q.revoke(); Q_EMIT q.authorityLost(); }
            else if (packet.packet.message == Message::JobStarted && channel.live()) channel.send(Message::RevokeJob, packet.packet.job);
            // Terminal acknowledgments for already retired IDs are idempotent;
            // replying to each one would create an endless Revoke/Revoked loop.
            return;
        }
        if (packet.packet.message == Message::Error || packet.packet.message == Message::JobRevoked) { fail(token); retire(token); return; }
        if (packet.packet.message != Message::JobStarted || job->started || packet.fds.size() != 2 || !live(*job) || !requests.live(token)
            || !pipeEnd(packet.fds[0], O_WRONLY) || !pipeEnd(packet.fds[1], O_RDONLY)) { fail(token); retire(token); return; }
        const auto fds = packet.takeFds(); job->writeFd = fds[0]; job->readFd = fds[1]; job->started = true;
        job->reader = new QSocketNotifier(job->readFd, QSocketNotifier::Read, &q);
        job->writer = new QSocketNotifier(job->writeFd, QSocketNotifier::Write, &q);
        QObject::connect(job->reader, &QSocketNotifier::activated, &q, [this, token] { output(token); });
        QObject::connect(job->writer, &QSocketNotifier::activated, &q, [this, token] { input(token); });
        QTimer::singleShot(2000, &q, [this, token] { if (const auto j = jobs.value(token); j && !j->input.isEmpty()) { fail(token); retire(token); } });
        input(token);
    }
    void input(RequestToken token) {
        const auto job = jobs.value(token); if (!job) return;
        if (!live(*job)) { fail(token); retire(token); return; }
        while (!job->input.isEmpty()) {
            const auto count = ::write(job->writeFd, job->input.constData(), static_cast<size_t>(job->input.size()));
            if (count > 0) { job->input.remove(0, count); continue; }
            if (count < 0 && errno == EINTR) continue;
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
            fail(token); retire(token); return;
        }
        job->writer->setEnabled(false); // Keep pipe open as independent helper lifetime.
    }
    void output(RequestToken token) {
        // Consume pending revocation before interpreting result bytes. Callbacks
        // may remove this token, so acquire its shared job only afterwards.
        channel.reconcile(); const auto job = jobs.value(token); if (!job) return;
        if (!live(*job)) { fail(token); retire(token); return; }
        char bytes[4096];
        while (true) {
            const auto count = ::read(job->readFd, bytes, sizeof(bytes));
            if (count > 0) { job->output.append(bytes, count); if (job->output.size() > 16384 || job->published) { fail(token); retire(token); return; } continue; }
            if (count < 0 && errno == EINTR) continue;
            if (count == 0 || (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) { fail(token); retire(token); return; }
            break;
        }
        const auto end = job->output.indexOf('\n'); if (end < 0) return;
        if (end != job->output.size()-1 || !requests.live(token)) { fail(token); retire(token); return; }
        QJsonParseError error; const auto object = QJsonDocument::fromJson(job->output.left(end), &error).object(); job->output.clear();
        if (error.error != QJsonParseError::NoError || object.size() != 2 || !object.value("results").isObject() || !object.value("response").isDouble()
            || object.value("response").toDouble() != object.value("response").toInt(-1)) { fail(token); retire(token); return; }
        const int response = object.value("response").toInt(-1); const auto results = captureResults(job->request.kind, object.value("results").toObject(), job->directory->path());
        if (!live(*job) || response < 0 || response > 2 || (response == 0 && (!results || !validCapturePublication(job->request.kind, *results)))) { fail(token); retire(token); return; }
        if (response == 0 && job->request.kind == CaptureKind::Screenshot) {
            struct stat file{}; const auto path = QFile::encodeName(job->directory->filePath("screenshot.png"));
            if (lstat(path.constData(), &file) || !S_ISREG(file.st_mode) || file.st_uid != geteuid() || (file.st_mode & 0077) || file.st_nlink != 1 || file.st_size <= 0 || file.st_size > 134217728) { fail(token); retire(token); return; }
        }
        job->published = true;
        Q_EMIT q.completed(token, static_cast<RequestResponse>(response), response == 0 ? *results : QVariantMap{});
        if (!jobs.contains(token)) return;
        if (response != 0 || job->request.kind == CaptureKind::Color) retire(token);
        else if (job->request.kind == CaptureKind::Screenshot) QTimer::singleShot(300000, &q, [this, token] { retire(token); });
    }
};
AuthorityCapture::AuthorityCapture(RequestRegistry &requests, QDBusConnection bus, QString runtime, int fd, QObject *parent)
    : CaptureUI(parent), d(std::make_unique<Private>(*this, requests, std::move(bus), std::move(runtime), fd)) { d->start(); }
AuthorityCapture::~AuthorityCapture() { revoke(); d->channel.stop(); }
bool AuthorityCapture::available() const { return d->channel.available(); }
bool AuthorityCapture::admitted() const { return d->channel.live() && d->admission.admitted(); }
void AuthorityCapture::request(RequestToken token, const CaptureRequest &request) {
    if (!token || !admitted() || !d->requests.live(token) || d->jobs.size() >= CaptureAuthority::Wire::MaxJobs || d->jobs.contains(token) || d->next == std::numeric_limits<quint64>::max()) { d->fail(token); return; }
    if (!d->bus.interface()) { d->fail(token); return; }
    const QDBusReply<uint> uid = d->bus.interface()->serviceUid(request.caller), pid = d->bus.interface()->servicePid(request.caller);
    if (!uid.isValid() || uid.value() != geteuid() || !pid.isValid() || !pid.value()) { d->fail(token); return; }
    auto job = std::make_shared<Private::Job>(); job->id = ++d->next; job->request = request; job->frontend = d->requests.frontendOwner();
    job->callerPidfd = static_cast<int>(syscall(SYS_pidfd_open, pid.value(), 0)); job->directory = std::make_unique<QTemporaryDir>(QDir(d->runtime).filePath("portal-capture-XXXXXX"));
    if (!pidAlive(job->callerPidfd) || !job->directory->isValid() || !d->live(*job)) { d->fail(token); return; }
    auto frame = captureFrame(request, job->directory->path(), d->channel.owner());
#ifdef QINDAQT_CAPTURE_ACTUAL_INPUT_TEST
    if (!augmentCaptureTestFrame(frame)) { d->fail(token); return; }
#endif
    job->input = QJsonDocument(frame).toJson(QJsonDocument::Compact) + '\n';
    if (job->input.size() > 16384) { d->fail(token); return; }
    d->jobs.insert(token, job);
    const auto scope = request.kind == CaptureKind::Stream ? CaptureAuthority::Wire::Scope::ScreenCast : CaptureAuthority::Wire::Scope::Screenshot;
    if (!d->channel.send(CaptureAuthority::Wire::Message::StartJob, job->id, CaptureAuthority::startPayload(scope, job->frontend, request.caller))) { d->fail(token); d->retire(token); }
}
void AuthorityCapture::cancel(RequestToken token) { d->retire(token); }
void AuthorityCapture::stop(const QString &session) { const auto tokens = d->jobs.keys(); for (const auto token : tokens) if (const auto j = d->jobs.value(token); j && j->request.session == session) d->retire(token); }
void AuthorityCapture::revoke() { const auto tokens = d->jobs.keys(); for (const auto token : tokens) { d->fail(token); d->retire(token); } }
}
