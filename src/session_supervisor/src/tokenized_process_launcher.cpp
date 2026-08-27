// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session_supervisor/tokenized_process_launcher.h"

#include "qindaqt/services/notification_presentation/presentation_token_channel.h"

#include <QProcess>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <utility>

namespace QindaQt::SessionSupervisor {
namespace {

void setError(QString *error, QString message)
{
    if (error != nullptr) {
        *error = std::move(message);
    }
}

} // namespace

bool TokenizedProcessLauncher::start(
    QProcess &process, const QString &program, QStringList arguments,
    const Services::NotificationPresentation::PresentationAccessToken &token,
    QString *error)
{
    if (process.state() != QProcess::NotRunning || program.trimmed().isEmpty()) {
        setError(error, QStringLiteral("tokenized child process request is invalid"));
        return false;
    }
    int descriptors[2] = {-1, -1};
    if (::pipe2(descriptors, O_CLOEXEC) != 0) {
        setError(error, QStringLiteral("could not create presentation token channel: %1")
                            .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }
    const int readDescriptor = descriptors[0];
    QString channelError;
    if (!Services::NotificationPresentation::PresentationTokenChannel::writeAndClose(
            descriptors[1], token, &channelError)) {
        ::close(readDescriptor);
        setError(error, std::move(channelError));
        return false;
    }

    arguments.append({QStringLiteral("--presentation-token-fd"),
                      QString::number(readDescriptor)});
    const pid_t supervisorProcessId = ::getpid();
    process.setChildProcessModifier([readDescriptor, supervisorProcessId] {
        // Only async-signal-safe syscalls are permitted between fork and exec.
        // AGENT-GUARD: Both essential children must die with qindaqt-session;
        // otherwise KWin death could leave a token-authenticated orphan that
        // later accepts an unrelated process reusing the compositor PID.
        if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0
            || ::getppid() != supervisorProcessId
            || ::fcntl(readDescriptor, F_SETFD, 0) != 0) {
            ::_exit(127);
        }
    });
    process.start(program, arguments);
    ::close(readDescriptor);
    if (!process.waitForStarted(StartTimeoutMilliseconds)) {
        setError(error,
                 QStringLiteral("could not start %1: %2")
                     .arg(program, process.errorString()));
        return false;
    }
    setError(error, {});
    return true;
}

} // namespace QindaQt::SessionSupervisor
