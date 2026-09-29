// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_prompt_provider.h"
#include "wire_types.h"
#include "session_display_binding.h"
#include <QProcessEnvironment>
#include <fcntl.h>
#include <unistd.h>
#include <QTimer>
#include <QFileInfo>
#include <algorithm>
#include <signal.h>
namespace qindaqt::keyring::service {
ProcessPromptProvider::ProcessPromptProvider(QString executable, QObject *parent,SessionDisplayBinding *display)
    : PromptProvider(parent), display_(display), executable_(std::move(executable)) {
    if (display_) connect(display_,&SessionDisplayBinding::revoked,this,[this] {
        while (!active_.empty()) finish(active_.begin()->first,true);
    });
}
bool ProcessPromptProvider::bindSessionDisplay(const QString &owner,const QString &name) {
    return display_ && display_->attach(owner,name);
}
ProcessPromptProvider::Active::~Active() { if (displayFd >= 0) close(displayFd); }
ProcessPromptProvider::~ProcessPromptProvider() {
    while (!active_.empty()) cancel(active_.begin()->first);
}
quint64 ProcessPromptProvider::begin(PromptRequest request, PromptCompletion done) {
    const auto id = ++generation_;
    if (active_.size() >= 4 || !QFileInfo(executable_).isExecutable()) {
        QTimer::singleShot(0,this,[done=std::move(done)]() mutable { done(SecureBuffer{},true); }); return 0;
    }
    auto active = std::make_unique<Active>();
    const int displayFd = display_ ? display_->openPromptConnection() : -2;
    if (displayFd == -1) {
        QTimer::singleShot(0,this,[done=std::move(done)]() mutable { done(SecureBuffer{},true); }); return 0;
    }
    active->displayFd = displayFd;
    active->process = std::make_unique<QProcess>();
    active->input = SecureBuffer(4097); active->done = std::move(done);
    auto *process = active->process.get();
    if (displayFd >= 0) {
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove("WAYLAND_SOCKET"); environment.remove("WAYLAND_DISPLAY");
        environment.insert("WAYLAND_SOCKET",QString::number(displayFd));
        environment.insert("WAYLAND_DISPLAY",display_->basename());
        process->setProcessEnvironment(environment);
        connect(process,&QProcess::started,this,[this,id] {
            const auto found = active_.find(id); if (found == active_.end()) return;
            if (found->second->displayFd >= 0) close(found->second->displayFd);
            found->second->displayFd = -1;
        });
    }
    process->setChildProcessModifier([displayFd] {
        sigset_t mask; sigemptyset(&mask); sigprocmask(SIG_SETMASK,&mask,nullptr);
        // AGENT-GUARD: inherit the exact validated connection, never reconnect by name.
        if (displayFd >= 0 && fcntl(displayFd,F_SETFD,0) != 0) _exit(2);
    });
    process->setStandardErrorFile(QProcess::nullDevice());
    process->setProcessChannelMode(QProcess::SeparateChannels);
    // Protocol is raw UTF-8 password bytes on stdout, at most 4096. No newline,
    // argv/env passwords or resident GUI. Helper exit 0 means explicit approval.
    connect(process,&QProcess::readyReadStandardOutput,this,[this,id] {
        const auto found = active_.find(id); if (found == active_.end()) return;
        auto &a = *found->second;
        while (a.process->bytesAvailable() > 0 && a.used < a.input.size()) {
            const auto n = a.process->read(reinterpret_cast<char *>(a.input.bytes().data()) + a.used,
                                           static_cast<qint64>(a.input.size() - a.used));
            if (n <= 0) break;
            a.used += static_cast<std::size_t>(n);
        }
        if (a.used > 4096) finish(id,true);
    });
    connect(process,&QProcess::finished,this,[this,id](int code,QProcess::ExitStatus status) {
        finish(id,code != 0 || status != QProcess::NormalExit);
    });
    connect(process,&QProcess::errorOccurred,this,[this,id](QProcess::ProcessError) { finish(id,true); });
    active_.emplace(id,std::move(active));
    process->start(executable_,{"--action",request.action,"--collection",request.collection,
                               "--label",request.label,"--caller",request.caller});
    QTimer::singleShot(30000,this,[this,id] { finish(id,true); });
    return id;
}
void ProcessPromptProvider::finish(quint64 id,bool cancelled) {
    const auto found = active_.find(id); if (found == active_.end()) return;
    auto a = std::move(found->second); active_.erase(found);
    if (display_ && !display_->live()) cancelled = true;
    a->process->disconnect(this);
    if (a->process->state() != QProcess::NotRunning) { a->process->kill(); a->process->waitForFinished(1000); }
    SecureBuffer password;
    if (!cancelled && a->used > 0 && a->used <= 4096) {
        password = SecureBuffer(a->used);
        std::copy_n(a->input.bytes().begin(),a->used,password.bytes().begin());
    } else cancelled = true;
    auto done = std::move(a->done);
    // QProcess signal stack must finish before retiring the process object.
    a->process.release()->deleteLater();
    done(std::move(password),cancelled);
}
void ProcessPromptProvider::cancel(quint64 id) {
    const auto found = active_.find(id); if (found == active_.end()) return;
    auto a = std::move(found->second); active_.erase(found);
    a->process->disconnect(this);
    if (a->process->state() != QProcess::NotRunning) { a->process->kill(); a->process->waitForFinished(1000); }
}
}
