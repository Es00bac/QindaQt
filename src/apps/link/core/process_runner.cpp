// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_runner.h"

#include <QtCore/QProcess>
#include <QtCore/QTimer>

#include <memory>

namespace QindaQt::LinkApp {

bool QProcessRunner::startDetached(const QString &program, const QStringList &arguments)
{
    return QProcess::startDetached(program, arguments);
}

void QProcessRunner::capture(const QString &program, const QStringList &arguments,
                             std::function<void(int, const QByteArray &)> done)
{
    auto *process = new QProcess(this);
    auto finished = std::make_shared<bool>(false);
    auto finish = [process, finished, done = std::move(done)](int code) {
        if (*finished) return;
        *finished = true;
        const QByteArray out = process->readAllStandardOutput();
        process->deleteLater();
        done(code, out);
    };
    connect(process, &QProcess::finished, process, [finish](int code, QProcess::ExitStatus status) {
        finish(status == QProcess::NormalExit ? code : -1);
    });
    connect(process, &QProcess::errorOccurred, process, [finish](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) finish(-1);
    });
    // The inventory asks the phone over ADB; a stuck phone must not stall the page forever.
    QTimer::singleShot(45000, process, [process, finish] {
        if (process->state() != QProcess::NotRunning) process->kill();  // our own child, by handle
        finish(-1);
    });
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->start(program, arguments);
}

bool FakeProcessRunner::startDetached(const QString &program, const QStringList &arguments)
{
    started.append(QStringList{program} + arguments);
    return startSucceeds;
}

void FakeProcessRunner::capture(const QString &program, const QStringList &arguments,
                                std::function<void(int, const QByteArray &)> done)
{
    captured.append(QStringList{program} + arguments);
    QTimer::singleShot(0, this, [done = std::move(done), out = captureOutput, code = captureExit] { done(code, out); });
}

} // namespace QindaQt::LinkApp
