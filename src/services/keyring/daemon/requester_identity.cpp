// SPDX-License-Identifier: GPL-3.0-or-later
#include "requester_identity.h"
#include <QDBusConnectionInterface>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <unistd.h>
namespace qindaqt::keyring::service {
namespace {
struct Process { quint32 parent = 0; quint64 started = 0; QString executable; };
Process process(quint32 pid) {
    if (!pid) return {};
    const auto base = QStringLiteral("/proc/%1/").arg(pid);
    QFile stat(base + "stat");
    if (!stat.open(QIODevice::ReadOnly)) return {};
    const auto bytes = stat.read(4097);
    if (bytes.size() > 4096) return {};
    const auto end = bytes.lastIndexOf(')');
    if (end < 0) return {};
    const auto fields = bytes.mid(end + 2).split(' ');
    if (fields.size() < 20) return {};
    bool validParent = false, validStart = false;
    Process result;
    result.parent = fields[1].toUInt(&validParent);
    result.started = fields[19].toULongLong(&validStart);
    const auto path = QFile::encodeName(base + "exe");
    char target[4097];
    const auto size = readlink(path.constData(), target, sizeof(target));
    if (!validParent || !validStart || !result.started) return {};
    // Protected same-user processes can hide /proc/PID/exe. Their bus PID/UID
    // and non-ptrace-gated start time remain usable; never invent a path/name.
    if (size <= 0 || size > 4096) return result;
    const QByteArray executable(target, static_cast<qsizetype>(size));
    result.executable = QString::fromUtf8(executable);
    if (result.executable.toUtf8() != executable || !result.executable.startsWith('/')) result.executable.clear();
    return result;
}
RequesterIdentity identify(quint32 pid, quint32 uid) {
    RequesterIdentity result;
    result.pid = pid; result.uid = uid;
    const auto before = process(pid);
    if (!before.started) return result;
    result.started = before.started; result.executable = before.executable;
    QSet<quint32> seen{pid};
    auto parent = before.parent;
    for (int depth = 0; parent > 1 && depth < 6 && !seen.contains(parent); ++depth) {
        seen.insert(parent);
        // Parentage is observed context, never proof of which agent/task
        // authorized the request. Do not cross into another user's processes.
        if (QFileInfo(QStringLiteral("/proc/%1").arg(parent)).ownerId() != uid) break;
        const auto value = process(parent);
        const auto checked = process(parent);
        if (!value.started || value.executable.isEmpty() || value.started != checked.started || value.executable != checked.executable) break;
        const auto description = QStringLiteral("%1 (PID %2)").arg(value.executable).arg(parent);
        if (description.toUtf8().size() > 1024) break;
        result.parents.append(description);
        parent = value.parent;
    }
    const auto after = process(pid);
    if (after.started != before.started || after.executable != before.executable) {
        result.started = 0; result.executable.clear(); result.parents.clear();
    }
    return result;
}
}
RequesterIdentity identifyRequester(const QDBusConnection &bus, const QString &owner) {
    if (!bus.interface() || !owner.startsWith(':')) return {};
    const auto pid = bus.interface()->servicePid(owner);
    const auto uid = bus.interface()->serviceUid(owner);
    if (!pid.isValid() || !uid.isValid() || uid.value() != geteuid()) return {};
    return identify(pid.value(), uid.value());
}
RequesterIdentity identifyLocalRequester() { return identify(static_cast<quint32>(getpid()), geteuid()); }
bool requesterUnchanged(const QDBusConnection &bus, const QString &owner, const RequesterIdentity &expected) {
    if (!expected.started || !bus.interface()) return false;
    const auto pid = bus.interface()->servicePid(owner);
    const auto uid = bus.interface()->serviceUid(owner);
    if (!pid.isValid() || !uid.isValid() || pid.value() != expected.pid || uid.value() != expected.uid) return false;
    const auto current = process(expected.pid);
    return current.started == expected.started && current.executable == expected.executable;
}
void describeRequester(protocol::PromptMetadata &metadata, const RequesterIdentity &requester) {
    metadata.processId = requester.started ? requester.pid : 0; metadata.userId = requester.uid;
    metadata.executable = requester.executable; metadata.parents = requester.parents;
}
}
