// SPDX-License-Identifier: LGPL-3.0-or-later
#include <qindaqt/services/application_uri/application_uri_opener.h>
#include <qindaqt/shell_launcher/launch_execution.h>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>
#include <fcntl.h>
#include <unistd.h>
namespace QindaQt::Services::ApplicationUri {
using namespace QindaQt::Shell::Launcher;
using namespace QindaQt::Apps::SettingsDefaultApps;
class DefaultApplicationUriOpener::Private {
public:
    struct Pending { QUrl uri; QString activation; QProcess *relay = nullptr; int fd = -1; QByteArray output; };
    DefaultApplicationUriOpener &q; DefaultApplicationsStore &store;
    QindaQt::ApplicationCatalog::DirectoryScan applications; QString executable;
    std::function<bool(quint64)> admission; std::function<int()> display;
    QHash<quint64, Pending> pending;
    Private(DefaultApplicationUriOpener &object, DefaultApplicationsStore &policy,
        QindaQt::ApplicationCatalog::DirectoryScan scan, QString relay,
        std::function<bool(quint64)> admitted, std::function<int()> open)
        : q(object), store(policy), applications(std::move(scan)), executable(std::move(relay)),
          admission(std::move(admitted)), display(std::move(open)) {}
    void finish(quint64 token, UriOpenResult result) {
        const auto it = pending.find(token); if (it == pending.end()) return;
        const auto entry = std::move(it.value()); pending.erase(it);
        if (entry.fd >= 0) close(entry.fd);
        if (entry.relay) {
            entry.relay->disconnect(&q);
            if (entry.relay->state() != QProcess::NotRunning) {
                entry.relay->kill(); entry.relay->waitForFinished(1000);
            }
            entry.relay->deleteLater();
        }
        if (result == UriOpenResult::Started && (!admission || !admission(token))) result = UriOpenResult::Unavailable;
        Q_EMIT q.completed(token, result);
    }
    void launch(quint64 token) {
        auto it = pending.find(token); if (it == pending.end()) return;
        if (!admission || !admission(token) || !display || executable.isEmpty()) { finish(token, UriOpenResult::Unavailable); return; }
        MimeTypeHandlers handlers; QString error;
        if (!store.loadMimeTypeHandlers(QStringLiteral("x-scheme-handler/") + it->uri.scheme().toLower(), &handlers, &error)
            || handlers.defaultDesktopId.isEmpty() || !handlers.defaultDesktopId.endsWith(QStringLiteral(".desktop"))) {
            finish(token, UriOpenResult::NoHandler); return;
        }
        const auto *application = applications.application(handlers.defaultDesktopId.chopped(8));
        if (!application) { finish(token, UriOpenResult::NoHandler); return; }
        const auto keys = LaunchExecutionParser::parse(application->documentText);
        if (!keys.ok() || keys.keys->terminal || keys.keys->dbusActivatable) { finish(token, UriOpenResult::UnsupportedHandler); return; }
        const auto encoded = it->uri.toString(QUrl::FullyEncoded);
        ExecExpansionValues values{application->entry.name, application->entry.iconName, application->desktopFilePath, {}, {encoded}};
        const auto plan = ExecFieldCodeExpander::expand(keys.keys->exec, values);
        if (!plan.ok() || plan.plan->fileArguments != 1) { finish(token, UriOpenResult::UnsupportedHandler); return; }
        QString program = plan.plan->program;
        if (!QDir::isAbsolutePath(program)) {
            if (program.contains(QLatin1Char('/'))) { finish(token, UriOpenResult::UnsupportedHandler); return; }
            program = QStandardPaths::findExecutable(program);
        }
        if (program.isEmpty() || !QFileInfo(program).isExecutable()
            || (!keys.keys->path.isEmpty() && (!QDir::isAbsolutePath(keys.keys->path) || !QFileInfo(keys.keys->path).isDir()))) {
            finish(token, UriOpenResult::Failed); return;
        }
        const int fd = display(); if (fd < 0 || !admission(token)) { if (fd >= 0) close(fd); finish(token, UriOpenResult::Unavailable); return; }
        it->fd = fd; auto *relay = new QProcess(&q); it->relay = relay;
        auto env = QProcessEnvironment::systemEnvironment(); env.remove(QStringLiteral("WAYLAND_DISPLAY")); env.remove(QStringLiteral("DISPLAY"));
        env.insert(QStringLiteral("WAYLAND_SOCKET"), QString::number(fd)); env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("wayland"));
        env.remove(QStringLiteral("XDG_ACTIVATION_TOKEN")); if (!it->activation.isEmpty()) env.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), it->activation);
        relay->setProcessEnvironment(env); relay->setChildProcessModifier([fd] { if (fcntl(fd, F_SETFD, 0) < 0) _exit(2); });
        QObject::connect(relay, &QProcess::started, &q, [this, token] {
            auto found = pending.find(token); if (found != pending.end() && found->fd >= 0) { close(found->fd); found->fd = -1; }
        });
        QObject::connect(relay, &QProcess::readyReadStandardOutput, &q, [this, token] {
            auto found = pending.find(token); if (found == pending.end()) return;
            found->output += found->relay->readAllStandardOutput();
            if (found->output.size() > 8) finish(token, UriOpenResult::Failed);
        });
        QObject::connect(relay, &QProcess::readyReadStandardError, &q, [relay] { relay->readAllStandardError(); });
        QObject::connect(relay, &QProcess::errorOccurred, &q, [this, token](QProcess::ProcessError e) { if (e == QProcess::FailedToStart) finish(token, UriOpenResult::Failed); });
        QObject::connect(relay, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), &q,
            [this, token](int code, QProcess::ExitStatus status) {
                auto found = pending.find(token); if (found == pending.end()) return;
                found->output += found->relay->readAllStandardOutput();
                finish(token, code == 0 && status == QProcess::NormalExit && found->output == "1\n" ? UriOpenResult::Started : UriOpenResult::Failed);
            });
        QJsonArray arguments; for (const auto &argument : plan.plan->arguments) arguments.append(argument);
        const auto payload = QJsonDocument(QJsonObject{{QStringLiteral("program"), program}, {QStringLiteral("arguments"), arguments},
            {QStringLiteral("directory"), keys.keys->path}}).toJson(QJsonDocument::Compact);
        relay->start(executable, {}); relay->write(payload); relay->closeWriteChannel();
        QTimer::singleShot(5000, &q, [this, token] { if (pending.contains(token)) finish(token, UriOpenResult::Failed); });
    }
};
DefaultApplicationUriOpener::DefaultApplicationUriOpener(DefaultApplicationsStore &store,
    QindaQt::ApplicationCatalog::DirectoryScan applications, QString relay,
    std::function<bool(quint64)> admission, std::function<int()> display, QObject *parent)
    : ApplicationUriOpener(parent), d(std::make_unique<Private>(*this, store, std::move(applications), std::move(relay), std::move(admission), std::move(display))) {}
DefaultApplicationUriOpener::~DefaultApplicationUriOpener() { const auto tokens = d->pending.keys(); for (const auto token : tokens) cancel(token); }
void DefaultApplicationUriOpener::setApplications(QindaQt::ApplicationCatalog::DirectoryScan applications) { d->applications = std::move(applications); }
void DefaultApplicationUriOpener::open(quint64 token, const QUrl &uri, const QString &activation) {
    if (d->pending.contains(token)) return;
    if (!token || d->pending.size() >= 16 || !uri.isValid() || uri.isRelative()
        || uri.scheme().isEmpty() || uri.toString(QUrl::FullyEncoded).size() > 8192 || activation.size() > 512) {
        QTimer::singleShot(0, this, [this, token] { Q_EMIT completed(token, UriOpenResult::Failed); }); return;
    }
    d->pending.insert(token, {uri, activation, nullptr, -1, {}});
    QTimer::singleShot(0, this, [this, token] { d->launch(token); });
}
void DefaultApplicationUriOpener::cancel(quint64 token) { d->finish(token, UriOpenResult::Cancelled); }
} // namespace QindaQt::Services::ApplicationUri
