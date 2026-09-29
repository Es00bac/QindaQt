// SPDX-License-Identifier: GPL-3.0-or-later
#include "resident_service_refresh.h"
#include "systemd_manager_port.h"
#include <QDebug>
#include <QElapsedTimer>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>
#include <QtDBus/QDBusMessage>

#include <algorithm>
#include <optional>

namespace QindaQt::SessionSupervisor {
namespace {
constexpr int RestartTimeoutMilliseconds = 2'000;
constexpr int AudioOwnerRetirementTimeoutMilliseconds = 2'000;
constexpr int AudioOwnerQueryTimeoutMilliseconds = 500;
constexpr auto AudioServiceName = "org.qindaqt.Audio1";
constexpr auto AudioUnitName = "qindaqt-audio-service.service";

std::optional<QString> audioOwner(const QDBusConnection &bus,
                                   const int timeoutMilliseconds = AudioOwnerQueryTimeoutMilliseconds)
{
    QDBusMessage query = QDBusMessage::createMethodCall(
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
        QStringLiteral("org.freedesktop.DBus"), QStringLiteral("GetNameOwner"));
    query.setArguments({QString::fromLatin1(AudioServiceName)});
    const QDBusMessage reply =
        bus.call(query, QDBus::Block, timeoutMilliseconds);
    if (reply.type() != QDBusMessage::ErrorMessage) {
        const auto arguments = reply.arguments();
        if (arguments.size() == 1
            && arguments.constFirst().metaType().id() == QMetaType::QString) {
            return arguments.constFirst().toString();
        }
        return std::nullopt;
    }
    if (reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner"))
        return QString{};
    return std::nullopt;
}

// AGENT-CONTRACT: qindaqt-session calls this before desktop consumers start;
// Settings' Audio1 client must not call a prior package owner after the
// resident service's fixed D-Bus structures may have changed.
bool waitForAudioOwnerRetirement(const QDBusConnection &bus, const QString &previousOwner)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < AudioOwnerRetirementTimeoutMilliseconds) {
        const int remainingMilliseconds =
            AudioOwnerRetirementTimeoutMilliseconds - static_cast<int>(timer.elapsed());
        const auto currentOwner = audioOwner(
            bus, std::min(AudioOwnerQueryTimeoutMilliseconds, remainingMilliseconds));
        if (!currentOwner.has_value()) return false;
        if (*currentOwner != previousOwner) return true;
        QThread::msleep(25);
    }
    return false;
}

// Sends one RestartUnit call along the resolved manager route. Returns false
// when the manager is unreachable or the call fails.
bool restartUnitOnManager(const SystemdManagerRoute &route, const QDBusConnection &bus,
                          const QString &unitName)
{
    const auto makeCall = [&unitName] {
        QDBusMessage restart = QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.systemd1"), QStringLiteral("/org/freedesktop/systemd1"),
            QStringLiteral("org.freedesktop.systemd1.Manager"), QStringLiteral("RestartUnit"));
        restart.setArguments({unitName, QStringLiteral("replace")});
        return restart;
    };
    if (route.kind == SystemdManagerRoute::Kind::SessionBusName) {
        return bus.call(makeCall(), QDBus::Block, RestartTimeoutMilliseconds).type()
            != QDBusMessage::ErrorMessage;
    }
    if (route.kind == SystemdManagerRoute::Kind::Native) {
        return nativeRestartUnit(route.address, unitName, route.requiresBusHello);
    }
    return false;
}
}

QStringList residentServiceRefreshUnits()
{
    return {
        QString::fromLatin1(AudioUnitName),
        QStringLiteral("qindaqt-clipboard-host.service"),
        QStringLiteral("qindaqt-display-service.service"),
        QStringLiteral("plasma-xdg-desktop-portal-kde.service"),
        QStringLiteral("xdg-desktop-portal.service"),
    };
}

bool refreshResidentServices(const QDBusConnection &bus,
                             const QStringList &unitNames,
                             const QString &systemdPrivateSocketPath,
                                  const SessionActivationScope scope)
{
    if (scope == SessionActivationScope::Private && systemdPrivateSocketPath.isEmpty()) return true;
    const SystemdManagerRoute route = resolveSystemdManagerRoute(bus, systemdPrivateSocketPath, scope);
    bool audioSafeToProceed = true;
    for (const QString &unitName : unitNames) {
        if (unitName.isEmpty()) continue;
        const bool audioUnit = unitName == QString::fromLatin1(AudioUnitName);
        std::optional<QString> previousAudioOwner;
        if (audioUnit) {
            previousAudioOwner = audioOwner(bus);
            if (!previousAudioOwner.has_value()) {
                qWarning("Could not determine the current Audio1 owner before refresh");
                audioSafeToProceed = false;
            }
        }
        const bool requested = restartUnitOnManager(route, bus, unitName);
        if (!requested) {
            qWarning("Could not restart resident session unit %s", qUtf8Printable(unitName));
            if (audioUnit && previousAudioOwner.has_value()
                && !previousAudioOwner->isEmpty()) {
                audioSafeToProceed = false;
            }
            continue;
        }
        if (audioUnit && previousAudioOwner.has_value() && !previousAudioOwner->isEmpty()
            && !waitForAudioOwnerRetirement(bus, *previousAudioOwner)) {
            qWarning("Timed out waiting for the previous Audio1 owner to retire");
            audioSafeToProceed = false;
        }
    }
    return audioSafeToProceed;
}

QStringList pipeWireStackUnits()
{
    return {
        QStringLiteral("pipewire.service"),
        QStringLiteral("wireplumber.service"),
        QStringLiteral("pipewire-pulse.service"),
    };
}

bool pipeWireAnswers(const int timeoutMilliseconds)
{
    const QString client = QStandardPaths::findExecutable(QStringLiteral("pw-cli"));
    if (client.isEmpty()) {
        return true;
    }
    QProcess probe;
    probe.setProgram(client);
    probe.setArguments({QStringLiteral("info"), QStringLiteral("0")});
    probe.setStandardOutputFile(QProcess::nullDevice());
    probe.setStandardErrorFile(QProcess::nullDevice());
    probe.start();
    if (!probe.waitForStarted(timeoutMilliseconds)) {
        return true;
    }
    if (!probe.waitForFinished(timeoutMilliseconds)) {
        // Connected but never answered: the hang that silenced dictation.
        probe.kill();
        static_cast<void>(probe.waitForFinished(1'000));
        return false;
    }
    return probe.exitStatus() == QProcess::NormalExit && probe.exitCode() == 0;
}

bool refreshUnresponsivePipeWire(const QDBusConnection &bus,
                                 const std::function<bool()> &answers,
                                 const QString &systemdPrivateSocketPath,
                                 const SessionActivationScope scope)
{
    if (scope == SessionActivationScope::Private && systemdPrivateSocketPath.isEmpty()) {
        return false;
    }
    if (!answers || answers()) {
        return false;
    }
    qWarning("PipeWire did not answer a client; restarting the audio stack");
    const SystemdManagerRoute route =
        resolveSystemdManagerRoute(bus, systemdPrivateSocketPath, scope);
    bool requested = false;
    for (const QString &unitName : pipeWireStackUnits()) {
        if (restartUnitOnManager(route, bus, unitName)) {
            requested = true;
        } else {
            qWarning("Could not restart audio unit %s", qUtf8Printable(unitName));
        }
    }
    return requested;
}

} // namespace QindaQt::SessionSupervisor
