// SPDX-License-Identifier: GPL-3.0-or-later
#include "systemd_manager_port.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QtDBus/QDBusConnectionInterface>

#include <systemd/sd-bus.h>

#include <cstring>
#include <vector>

namespace QindaQt::SessionSupervisor {
namespace {

constexpr unsigned long long kManagerCallTimeoutUsec = 2'000'000; // 2s, matching the QtDBus blocks

// Opens an sd-bus connection to `address`, or to the manager's default user
// endpoint when the address is empty. sd-bus (not QtDBus) is required because
// the manager's private endpoint is not a message bus and never answers
// Hello; sd-bus speaks exactly that direct-connection protocol.
// `requiresBusHello` is set only for hermetic-test bus endpoints, where the
// fake manager daemon requires the client Hello handshake before calls.
sd_bus *openManagerBus(const QString &address, const bool requiresBusHello)
{
    sd_bus *bus = nullptr;
    if (sd_bus_new(&bus) < 0 || bus == nullptr) {
        return nullptr;
    }
    if (requiresBusHello) {
        sd_bus_set_bus_client(bus, 1);
    }
    const QByteArray native = address.toUtf8();
    if (!native.isEmpty() && sd_bus_set_address(bus, native.constData()) < 0) {
        sd_bus_unref(bus);
        return nullptr;
    }
    if (sd_bus_start(bus) < 0) {
        sd_bus_unref(bus);
        return nullptr;
    }
    return bus;
}

[[nodiscard]] bool callManager(const QString &address, const bool requiresBusHello,
                               const char *member, const char *signature,
                               const QStringList &arguments)
{
    sd_bus *bus = openManagerBus(address, requiresBusHello);
    if (bus == nullptr) {
        return false;
    }
    sd_bus_message *message = nullptr;
    int result = sd_bus_message_new_method_call(
        bus, &message, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", member);
    if (result < 0) {
        sd_bus_unref(bus);
        return false;
    }
    if (std::strcmp(signature, "as") == 0) {
        std::vector<QByteArray> storage;
        std::vector<char *> pointers;
        storage.reserve(static_cast<std::size_t>(arguments.size()));
        pointers.reserve(static_cast<std::size_t>(arguments.size()) + 1);
        for (const QString &argument : arguments) {
            storage.push_back(argument.toUtf8());
        }
        for (QByteArray &value : storage) {
            pointers.push_back(value.data());
        }
        pointers.push_back(nullptr);
        result = sd_bus_message_append_strv(message, pointers.data());
    } else {
        for (const QString &argument : arguments) {
            const QByteArray value = argument.toUtf8();
            result = sd_bus_message_append_basic(message, 's', value.constData());
            if (result < 0) {
                break;
            }
        }
    }
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = nullptr;
    if (result >= 0) {
        result = sd_bus_call(bus, message, kManagerCallTimeoutUsec, &error, &reply);
    }
    const bool sent = result >= 0;
    sd_bus_error_free(&error);
    if (reply != nullptr) {
        sd_bus_message_unref(reply);
    }
    sd_bus_message_unref(message);
    sd_bus_unref(bus);
    return sent;
}

} // namespace

SystemdManagerRoute resolveSystemdManagerRoute(const QDBusConnection &sessionBus,
                                               const QString &privateSocketPath)
{
    const QString socketPath = privateSocketPath.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
              + QStringLiteral("/systemd/private")
        : privateSocketPath;
    if (!socketPath.isEmpty() && QFileInfo::exists(socketPath)) {
        return {SystemdManagerRoute::Kind::Native,
                QStringLiteral("unix:path=") + socketPath,
                !privateSocketPath.isEmpty()};
    }
    // AGENT-GUARD: use the session-bus name only when a manager actually owns
    // it; on a private (dbus-run-session) bus the name is unowned and a call
    // would activate /usr/share/dbus-1's second-user-manager entry, which
    // exits with status 1 (the production failure).
    if (sessionBus.isConnected() && sessionBus.interface() != nullptr
        && sessionBus.interface()->isServiceRegistered(
            QStringLiteral("org.freedesktop.systemd1"))) {
        return {SystemdManagerRoute::Kind::SessionBusName, {}};
    }
    return {SystemdManagerRoute::Kind::Unavailable, {}};
}

bool nativeSetManagerEnvironment(const QString &address, const QStringList &assignments,
                                   const bool requiresBusHello)
{
    return callManager(address, requiresBusHello, "SetEnvironment", "as", assignments);
}

bool nativeRestartUnit(const QString &address, const QString &unitName,
                       const bool requiresBusHello)
{
    return callManager(address, requiresBusHello, "RestartUnit", "ss",
                       {unitName, QStringLiteral("replace")});
}

} // namespace QindaQt::SessionSupervisor
