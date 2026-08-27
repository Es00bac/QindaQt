// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/session_supervisor/direct_parent_process.h"

#include <QString>

#include <cerrno>
#include <cstring>
#include <limits>
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

bool isUsableCompositorProcessId(qint64 processId) noexcept
{
    return processId > 1
        && processId <= static_cast<qint64>(std::numeric_limits<pid_t>::max());
}

std::optional<qint64> establishDirectParentProcessWitness(QString *error)
{
    const qint64 firstRead = static_cast<qint64>(::getppid());
    if (!isUsableCompositorProcessId(firstRead)) {
        setError(error,
                 QStringLiteral("qindaqt-session requires a stable direct compositor parent"));
        return std::nullopt;
    }
    // SIGKILL is deliberate: inherited signal masks or future graceful-shutdown
    // handlers must not let notification authority survive compositor death.
    if (::prctl(PR_SET_PDEATHSIG, SIGKILL) != 0) {
        setError(error,
                 QStringLiteral("could not establish compositor lifetime witness: %1")
                     .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return std::nullopt;
    }
    const qint64 secondRead = static_cast<qint64>(::getppid());
    // AGENT-GUARD: PR_SET_PDEATHSIG does not report a death that happened
    // immediately before it was armed. The mandatory recheck closes that race;
    // accepting the reaper would make later PID reuse look like the compositor.
    if (firstRead != secondRead) {
        setError(error,
                 QStringLiteral("direct compositor parent changed during witness setup"));
        return std::nullopt;
    }
    setError(error, {});
    return firstRead;
}

} // namespace QindaQt::SessionSupervisor
