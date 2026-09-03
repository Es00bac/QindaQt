// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/session_supervisor/supervised_process_launcher.h>

#include <QProcess>

#include <signal.h>
#include <sys/prctl.h>
#include <unistd.h>

namespace QindaQt::SessionSupervisor {

bool SupervisedProcessLauncher::start(QProcess &process, const QString &program,
                                      const QStringList &arguments, QString *error)
{
    if (process.state() != QProcess::NotRunning || program.trimmed().isEmpty()) {
        if (error != nullptr) {
            *error = QStringLiteral("supervised child process request is invalid");
        }
        return false;
    }
    const pid_t supervisorProcessId = ::getpid();
    process.setChildProcessModifier([supervisorProcessId] {
        if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0
            || ::getppid() != supervisorProcessId) {
            ::_exit(127);
        }
    });
    process.start(program, arguments);
    if (error != nullptr) {
        error->clear();
    }
    return true;
}

} // namespace QindaQt::SessionSupervisor
