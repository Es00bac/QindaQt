// SPDX-License-Identifier: GPL-3.0-or-later
#include "replaced_activation_owner.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QtDBus/QDBusMessage>

#include <algorithm>
#include <csignal>
#include <utility>
#include <cstddef>
#include <optional>
#include <sys/types.h>
#include <sys/syscall.h>
#include <poll.h>
#include <unistd.h>

namespace QindaQt::SessionSupervisor {
namespace {
constexpr int QueryTimeoutMilliseconds = 500;
constexpr int RetirementTimeoutMilliseconds = 2'000;

QDBusMessage busCall(const QString &member, const QString &name)
{
    QDBusMessage call = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), member);
    call.setArguments({name});
    return call;
}

// Unique owner name; empty when nobody owns it; nullopt when unknown.
std::optional<QString> ownerOf(const QDBusConnection &bus, const QString &name,
                                     const int timeout = QueryTimeoutMilliseconds)
{
    const QDBusMessage reply =
        bus.call(busCall(QStringLiteral("GetNameOwner"), name), QDBus::Block,
                 timeout);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner")) {
            return QString{};
        }
        return std::nullopt;
    }
    return reply.arguments().value(0).toString();
}

std::optional<qint64> ownerPid(const QDBusConnection &bus, const QString &name)
{
    const QDBusMessage reply =
        bus.call(busCall(QStringLiteral("GetConnectionUnixProcessID"), name), QDBus::Block,
                 QueryTimeoutMilliseconds);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return std::nullopt;
    }
    bool ok = false;
    const qint64 pid = reply.arguments().constFirst().toLongLong(&ok);
    return ok && pid > 1 ? std::optional<qint64>(pid) : std::nullopt;
}

bool waitForRetirement(const QDBusConnection &bus, const QString &name,
                       const QString &previousOwner)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < RetirementTimeoutMilliseconds) {
        const int remaining = RetirementTimeoutMilliseconds - static_cast<int>(timer.elapsed());
        const auto owner = ownerOf(bus, name, std::min(QueryTimeoutMilliseconds, remaining));
        if (owner.has_value() && *owner != previousOwner) {
            return true;
        }
        QThread::msleep(25);
    }
    return false;
}

std::optional<ReplacedOwnerIdentity> processIdentity(const qint64 pid, const QString &root)
{
    const QString directory = root + QLatin1Char('/') + QString::number(pid);
    const QFileInfo info(directory);
    if (!info.isDir() || info.ownerId() != ::getuid()) return std::nullopt;
    QFile stat(directory + QStringLiteral("/stat"));
    if (!stat.open(QIODevice::ReadOnly)) return std::nullopt;
    const QByteArray bytes = stat.read(4097);
    if (bytes.size() > 4096) return std::nullopt;
    // comm may contain spaces or ')'; fields after the last ')' start at3.
    bool pidValid = false;
    const qint64 statPid = bytes.left(bytes.indexOf(' ')).toLongLong(&pidValid);
    if (!pidValid || statPid != pid) return std::nullopt;
    const qsizetype close = bytes.lastIndexOf(')');
    if (close < 0) return std::nullopt;
    const auto fields = bytes.mid(close + 1).simplified().split(' ');
    bool valid = false;
    const quint64 startTime = fields.value(19).toULongLong(&valid);
    if (!valid || startTime == 0) return std::nullopt;
    char link[4096];
    const QByteArray path = QFile::encodeName(directory + QStringLiteral("/exe"));
    const ssize_t size = ::readlink(path.constData(), link, sizeof(link));
    if (size <= 0 || size >= static_cast<ssize_t>(sizeof(link))) return std::nullopt;
    return ReplacedOwnerIdentity{static_cast<quint32>(info.ownerId()), startTime,
                                QFile::decodeName(QByteArray(link, static_cast<qsizetype>(size)))};
}

class PidfdWitness final : public ReplacedOwnerProcessWitness {
public:
    PidfdWitness(const qint64 pid, QString root, const int fd)
        : m_pid(pid), m_root(std::move(root)), m_fd(fd) {}
    ~PidfdWitness() override { ::close(m_fd); }
    std::optional<ReplacedOwnerIdentity> identity() const override
    {
        pollfd life{m_fd, POLLIN, 0};
        if (::poll(&life, 1, 0) != 0) return std::nullopt;
        return processIdentity(m_pid, m_root);
    }
    bool terminate() override
    {
        pollfd life{m_fd, POLLIN, 0};
        if (::poll(&life, 1, 0) != 0) return false;
#ifdef SYS_pidfd_send_signal
        return ::syscall(SYS_pidfd_send_signal, m_fd, SIGTERM, nullptr, 0) == 0;
#else
        return false;
#endif
    }
private:
    qint64 m_pid;
    QString m_root;
    int m_fd;
};

std::unique_ptr<ReplacedOwnerProcessWitness> witnessFor(const qint64 pid, const QString &root)
{
    const auto before = processIdentity(pid, root);
    if (!before) return {};
#ifdef SYS_pidfd_open
    const int fd = static_cast<int>(::syscall(SYS_pidfd_open, static_cast<pid_t>(pid), 0));
    if (fd < 0) return {};
    auto witness = std::make_unique<PidfdWitness>(pid, root, fd);
    const auto after = witness->identity();
    // AGENT-GUARD: pidfd_open alone could capture a recycled PID. Both
    // observations must match before admitting this held process lifetime.
    if (!after || *after != *before) return {};
    return witness;
#else
    return {};
#endif
}

bool eligibleExecutable(const QString &name, const QString &executable)
{
    if (!executableWasReplaced(executable)) return false;
    if (name == QStringLiteral("org.qindaqt.Power1"))
        return executable == QStringLiteral(QINDAQT_POWER_SERVICE_INSTALL_PATH " (deleted)");
    return true;
}
} // namespace

QStringList replacedActivationServiceNames()
{
    return {
        QStringLiteral("org.qindaqt.Settings1"),
        QStringLiteral("org.qindaqt.Power1"),
        QStringLiteral("org.freedesktop.impl.portal.desktop.qindaqt"),
    };
}

bool executableWasReplaced(const QString &exeLinkTarget)
{
    return exeLinkTarget.endsWith(QStringLiteral(" (deleted)"));
}

QStringList retireReplacedActivationOwners(const QDBusConnection &bus,
                                           const QStringList &serviceNames,
                                           const SessionActivationScope scope,
                                           const QString &procRoot,
                                           const ReplacedOwnerWitnessFactory &witnessFactory)
{
    QStringList retired;
    if (scope == SessionActivationScope::Private) {
        return retired;
    }
    const QString root = procRoot.isEmpty() ? QStringLiteral("/proc") : procRoot;
    for (const QString &name : serviceNames) {
        const auto owner = ownerOf(bus, name);
        if (!owner.has_value() || owner->isEmpty()) {
            continue;
        }
        const auto pid = ownerPid(bus, *owner);
        if (!pid.has_value()) {
            qWarning("Could not identify the process that owns %s", qUtf8Printable(name));
            continue;
        }
        auto witness = witnessFactory ? witnessFactory(*pid, root) : witnessFor(*pid, root);
        if (!witness) continue;
        const auto identity = witness->identity();
        if (!identity || identity->userId != ::getuid()
            || !eligibleExecutable(name, identity->executable)) continue;
        // ADR0360: query failure is never signal authority. Resolve the same
        // unique owner/PID again while the kernel witness holds the process,
        // then re-read uid/starttime/raw executable immediately before signal.
        const auto currentOwner = ownerOf(bus, name);
        const auto currentPid = currentOwner && *currentOwner == *owner
            ? ownerPid(bus, *currentOwner) : std::nullopt;
        const auto currentIdentity = witness->identity();
        if (!currentPid || *currentPid != *pid || !currentIdentity
            || *currentIdentity != *identity) continue;
        qInfo("Retiring %s (pid %lld): its executable was replaced by an update",
              qUtf8Printable(name), static_cast<long long>(*pid));
        if (!witness->terminate()) {
            qWarning("Could not stop the outdated %s owner", qUtf8Printable(name));
            continue;
        }
        if (!waitForRetirement(bus, name, *owner)) {
            qWarning("Timed out waiting for the outdated %s owner to exit", qUtf8Printable(name));
            continue;
        }
        retired.append(name);
    }
    return retired;
}

} // namespace QindaQt::SessionSupervisor
