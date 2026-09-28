// SPDX-License-Identifier: GPL-3.0-or-later
#include "replaced_activation_owner.h"

#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QtDBus/QDBusMessage>

#include <csignal>
#include <cstddef>
#include <optional>
#include <sys/types.h>
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
std::optional<QString> ownerOf(const QDBusConnection &bus, const QString &name)
{
    const QDBusMessage reply =
        bus.call(busCall(QStringLiteral("GetNameOwner"), name), QDBus::Block,
                 QueryTimeoutMilliseconds);
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
        const auto owner = ownerOf(bus, name);
        if (owner.has_value() && *owner != previousOwner) {
            return true;
        }
        QThread::msleep(25);
    }
    return false;
}
} // namespace

QStringList replacedActivationServiceNames()
{
    return {
        QStringLiteral("org.qindaqt.Settings1"),
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
                                           const QString &procRoot)
{
    QStringList retired;
    if (scope == SessionActivationScope::Private && procRoot.isEmpty()) {
        return retired;
    }
    const QString root = procRoot.isEmpty() ? QStringLiteral("/proc") : procRoot;
    for (const QString &name : serviceNames) {
        const auto owner = ownerOf(bus, name);
        if (!owner.has_value() || owner->isEmpty()) {
            continue;
        }
        const auto pid = ownerPid(bus, name);
        if (!pid.has_value()) {
            qWarning("Could not identify the process that owns %s", qUtf8Printable(name));
            continue;
        }
        const QString processDirectory = root + QLatin1Char('/') + QString::number(*pid);
        // AGENT-GUARD: never signal another user's process, even one that
        // somehow owns a QindaQt name on this bus.
        if (QFileInfo(processDirectory).ownerId() != ::getuid()) {
            continue;
        }
        // Read the raw link text: QFile::symLinkTarget() canonicalizes and
        // drops the kernel's " (deleted)" marker this decision depends on.
        constexpr std::size_t LinkCapacity = 4096;
        char buffer[LinkCapacity];
        const QByteArray linkPath = QFile::encodeName(processDirectory + QStringLiteral("/exe"));
        const ssize_t length = ::readlink(linkPath.constData(), buffer, LinkCapacity - 1);
        if (length <= 0) {
            continue;
        }
        const QByteArray link(buffer, static_cast<qsizetype>(length));
        if (!executableWasReplaced(QFile::decodeName(link))) {
            continue;
        }
        qInfo("Retiring %s (pid %lld): its executable was replaced by an update",
              qUtf8Printable(name), static_cast<long long>(*pid));
        if (::kill(static_cast<pid_t>(*pid), SIGTERM) != 0) {
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
