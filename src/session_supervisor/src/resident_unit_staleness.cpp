// SPDX-License-Identifier: GPL-3.0-or-later
#include "resident_unit_staleness.h"
#include "replaced_activation_owner.h"

#include <QFile>

#include <climits>
#include <unistd.h>

namespace QindaQt::SessionSupervisor {
namespace {
QString processDirectory(qint64 pid, const QString &procRoot)
{
    return (procRoot.isEmpty() ? QStringLiteral("/proc") : procRoot) + QLatin1Char('/')
        + QString::number(pid);
}
}

ResidentUnitState classifyResidentUnit(const ResidentUnitInstance &instance,
                                       const std::optional<quint64> loginStartTicks)
{
    if (instance.mainPid == 0) return ResidentUnitState::NotRunning;
    if (instance.executableReplaced) return ResidentUnitState::ExecutableReplaced;
    if (!instance.startTicks || !loginStartTicks) return ResidentUnitState::Unobservable;
    return *instance.startTicks < *loginStartTicks ? ResidentUnitState::PredatesLogin
                                                   : ResidentUnitState::StartedThisLogin;
}

bool residentUnitIsStale(const ResidentUnitState state)
{
    return state == ResidentUnitState::PredatesLogin
        || state == ResidentUnitState::ExecutableReplaced;
}

const char *residentUnitStateName(const ResidentUnitState state)
{
    switch (state) {
    case ResidentUnitState::NotRunning: return "not running";
    case ResidentUnitState::Unobservable: return "unobservable";
    case ResidentUnitState::StartedThisLogin: return "started in this login";
    case ResidentUnitState::PredatesLogin: return "predates this login";
    case ResidentUnitState::ExecutableReplaced: return "executable replaced";
    }
    return "unknown";
}

std::optional<quint64> processStartTicks(const qint64 pid, const QString &procRoot)
{
    if (pid <= 0) return std::nullopt;
    QFile stat(processDirectory(pid, procRoot) + QStringLiteral("/stat"));
    if (!stat.open(QIODevice::ReadOnly)) return std::nullopt;
    const QByteArray bytes = stat.read(4097);
    // comm (field 2) may contain spaces and parentheses; fields resume after
    // the last ')'. starttime is field 22, the 20th after it.
    const qsizetype close = bytes.lastIndexOf(')');
    if (close < 0) return std::nullopt;
    const QList<QByteArray> fields = bytes.mid(close + 2).split(' ');
    bool valid = false;
    const quint64 ticks = fields.value(19).toULongLong(&valid);
    return valid ? std::optional(ticks) : std::nullopt;
}

bool processExecutableReplaced(const qint64 pid, const QString &procRoot)
{
    if (pid <= 0) return false;
    const QByteArray link =
        QFile::encodeName(processDirectory(pid, procRoot) + QStringLiteral("/exe"));
    char target[PATH_MAX + 16] = {};
    const ssize_t length = ::readlink(link.constData(), target, sizeof(target) - 1);
    if (length <= 0) return false;
    return executableWasReplaced(QFile::decodeName(QByteArray(target, length)));
}

} // namespace QindaQt::SessionSupervisor
