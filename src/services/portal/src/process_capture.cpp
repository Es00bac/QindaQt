// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/portal/process_capture.h>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QTimer>
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
namespace QindaQt::Services::Portal {
class ProcessCapture::Private {
public:
    struct Job {
        CaptureRequest request; QString frontend; QPointer<QProcess> process;
        std::unique_ptr<QTemporaryDir> directory; QByteArray output;
        int display = -1, capture = -1, pidfd = -1;
        bool published = false;
        ~Job() { for (const int fd : {display, capture, pidfd}) if (fd >= 0) ::close(fd); }
    };
    ProcessCapture &q; PortalSessionBinding &binding; AccessConsent &authority; RequestRegistry &requests;
    QDBusConnection bus; QString executable, runtime; QHash<RequestToken, std::shared_ptr<Job>> jobs; QTimer lifetime;
    Private(ProcessCapture &object, PortalSessionBinding &b, AccessConsent &a, RequestRegistry &r, QDBusConnection connection, QString helper, QString root)
        : q(object), binding(b), authority(a), requests(r), bus(std::move(connection)), executable(std::move(helper)), runtime(std::move(root)) {
        lifetime.setInterval(50);
        QObject::connect(&lifetime, &QTimer::timeout, &q, [this] {
            if (!q.admitted()) { q.revoke(); return; }
            const auto tokens = jobs.keys(); for (const auto token : tokens) if (const auto job = jobs.value(token); job && !live(*job)) { if (!job->published) fail(token); retire(token); }
        }); lifetime.start();
    }
    bool live(const Job &job) const {
        pollfd event{job.pidfd, POLLIN, 0};
        if (!q.admitted() || job.pidfd < 0 || poll(&event, 1, 0) != 0 || job.frontend != requests.frontendOwner()) return false;
        auto *daemon = bus.interface(); if (!daemon) return false;
        const QDBusReply<QString> caller = daemon->serviceOwner(job.request.caller);
        return caller.isValid() && caller.value() == job.request.caller;
    }
    void retire(RequestToken token) {
        const auto job = jobs.take(token); if (!job) return;
        if (job->process) {
            job->process->disconnect(&q);
            if (job->process->state() != QProcess::NotRunning) { QObject::connect(job->process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), job->process, &QObject::deleteLater); job->process->kill(); }
            else job->process->deleteLater();
        }
        if (!job->request.session.isEmpty()) Q_EMIT q.closed(job->request.session);
    }
    void fail(RequestToken token) { QTimer::singleShot(0, &q, [this, token] { Q_EMIT q.completed(token, RequestResponse::Failed, {}); }); }
    void output(RequestToken token) {
        const auto job = jobs.value(token); if (!job || !job->process) return;
        job->output += job->process->readAllStandardOutput();
        if (job->output.size() > 16384) { fail(token); retire(token); return; }
        const auto end = job->output.indexOf('\n'); if (end < 0) return;
        if (job->published || end != job->output.size() - 1) { fail(token); retire(token); return; }
        QJsonParseError error; const auto object = QJsonDocument::fromJson(job->output.left(end), &error).object(); job->output.clear();
        if (error.error != QJsonParseError::NoError || object.size() != 2 || !object.value("results").isObject()
            || !object.value("response").isDouble() || object.value("response").toDouble() != object.value("response").toInt(-1)) { fail(token); retire(token); return; }
        const int response = object.value("response").toInt(-1); const auto results = captureResults(job->request.kind, object.value("results").toObject(), job->directory->path());
        if (!requests.live(token) || !live(*job) || response < 0 || response > 2 || (response == 0 && (!results || !validCapturePublication(job->request.kind, *results)))) { fail(token); retire(token); return; }
        if (response == 0 && job->request.kind == CaptureKind::Screenshot) {
            const auto path = job->directory->filePath("screenshot.png"); struct stat file{};
            if (lstat(QFile::encodeName(path).constData(), &file) != 0 || !S_ISREG(file.st_mode) || file.st_uid != geteuid() || (file.st_mode & 0077) || file.st_nlink != 1 || file.st_size <= 0 || file.st_size > 134217728) { fail(token); retire(token); return; }
        }
        job->published = true;
        Q_EMIT q.completed(token, static_cast<RequestResponse>(response), response == 0 ? *results : QVariantMap{});
        // Completion may synchronously retire this job through registry callbacks.
        if (!jobs.contains(token)) return;
        if (response != 0 || job->request.kind == CaptureKind::Color) retire(token);
        else if (job->request.kind == CaptureKind::Screenshot) QTimer::singleShot(300000, &q, [this, token] { retire(token); });
    }
};
ProcessCapture::ProcessCapture(PortalSessionBinding &binding, AccessConsent &authority, RequestRegistry &requests,
    QDBusConnection bus, QString helper, QString runtime)
    : d(std::make_unique<Private>(*this, binding, authority, requests, std::move(bus), std::move(helper), std::move(runtime))) {
    connect(&authority, &AccessConsent::authorityLost, this, [this] { revoke(); Q_EMIT authorityLost(); });
    connect(&binding, &PortalSessionBinding::invalidated, this, [this] { revoke(); Q_EMIT authorityLost(); });
}
ProcessCapture::~ProcessCapture() {
    // Stop children before their temporary storage and borrowed authority disappear.
    const auto children = findChildren<QProcess *>(); for (auto *process : children) { process->disconnect(this); if (process->state() != QProcess::NotRunning) { process->kill(); process->waitForFinished(1000); } }
    d->jobs.clear();
}
bool ProcessCapture::admitted() const { return d->binding.live() && d->authority.admitted(); }
void ProcessCapture::request(RequestToken token, const CaptureRequest &request) {
    if (!token || !admitted() || !d->requests.live(token) || d->executable.isEmpty() || d->jobs.size() >= 16 || d->jobs.contains(token)) { d->fail(token); return; }
    auto *daemon = d->bus.interface(); if (!daemon) { d->fail(token); return; }
    const QDBusReply<uint> uid = daemon->serviceUid(request.caller), pid = daemon->servicePid(request.caller);
    if (!uid.isValid() || uid.value() != static_cast<uint>(geteuid()) || !pid.isValid() || !pid.value()) { d->fail(token); return; }
    auto job = std::make_shared<Private::Job>(); job->request = request; job->frontend = d->requests.frontendOwner();
    job->pidfd = static_cast<int>(syscall(SYS_pidfd_open, pid.value(), 0));
    job->directory = std::make_unique<QTemporaryDir>(QDir(d->runtime).filePath("portal-capture-XXXXXX"));
    job->display = d->binding.openDisplay(); job->capture = d->binding.openDisplay();
    if (job->pidfd < 0 || !job->directory->isValid() || job->display < 0 || job->capture < 0 || !d->live(*job)) { d->fail(token); return; }
    auto *process = new QProcess(this); job->process = process; const std::weak_ptr<Private::Job> weak = job;
    d->jobs.insert(token, job);
    auto env = QProcessEnvironment::systemEnvironment();
    for (const auto &name : {QStringLiteral("DISPLAY"), QStringLiteral("WAYLAND_DISPLAY"), QStringLiteral("QT_QPA_PLATFORMTHEME")}) env.remove(name);
    env.insert("QT_QPA_PLATFORM", "wayland"); env.insert("QT_STYLE_OVERRIDE", "Fusion"); env.insert("WAYLAND_SOCKET", QString::number(job->display)); env.insert("QINDAQT_CAPTURE_SOCKET", QString::number(job->capture));
    process->setProcessEnvironment(env);
    const int display = job->display, capture = job->capture;
    process->setChildProcessModifier([display, capture] { if (fcntl(display, F_SETFD, 0) < 0 || fcntl(capture, F_SETFD, 0) < 0) _exit(2); });
    connect(process, &QProcess::started, this, [weak] { if (const auto j = weak.lock()) { ::close(j->display); ::close(j->capture); j->display = j->capture = -1; } });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, token] { d->output(token); });
    connect(process, &QProcess::readyReadStandardError, this, [process] { process->readAllStandardError(); });
    connect(process, &QProcess::errorOccurred, this, [this, token](QProcess::ProcessError error) { if (error == QProcess::FailedToStart) { d->fail(token); d->retire(token); } });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, token, weak](int code, QProcess::ExitStatus status) {
        const auto j = weak.lock(); if (!j) return;
        d->output(token);
        if (!d->jobs.contains(token)) return;
        if (!j->published || code != 0 || status != QProcess::NormalExit || j->request.kind == CaptureKind::Stream) { if (!j->published) d->fail(token); d->retire(token); }
        else { j->process->deleteLater(); j->process = nullptr; }
    });
    process->start(d->executable, {});
    process->write(QJsonDocument(captureFrame(request, job->directory->path(), d->binding.compositorOwner())).toJson(QJsonDocument::Compact) + '\n');
}
void ProcessCapture::cancel(RequestToken token) { d->retire(token); }
void ProcessCapture::stop(const QString &session) { const auto tokens = d->jobs.keys(); for (const auto token : tokens) if (d->jobs.value(token)->request.session == session) d->retire(token); }
void ProcessCapture::revoke() { const auto tokens = d->jobs.keys(); for (const auto token : tokens) d->retire(token); }
}
