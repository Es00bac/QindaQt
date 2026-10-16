// SPDX-License-Identifier: GPL-3.0-or-later
#include "manager_calls.h"
#include <cstdarg>
#include <cstring>
#include <memory>
namespace qindaqt::keyring::pambridge {
namespace {
using Message = std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)>;
}
int callManager(sd_bus *bus, const ManagerCall &call, sd_bus_message **reply, const char *types, ...) {
    va_list arguments;
    va_start(arguments, types);
    const int status = sd_bus_call_methodv(bus, call.destination, call.path, call.interface, call.member,
                                           nullptr, reply, types, arguments);
    va_end(arguments);
    return status;
}
std::string managerUnitPath(sd_bus *bus, const std::string &unit, bool privileged) {
    sd_bus_message *raw = nullptr;
    if (callManager(bus, privileged ? LoadUnitCall : GetUnitCall, &raw, "s", unit.c_str()) < 0) return {};
    Message reply(raw, sd_bus_message_unref);
    const char *path = nullptr;
    return sd_bus_message_read(raw, "o", &path) > 0 ? std::string(path) : std::string();
}
bool managerStartUnit(sd_bus *bus, const std::string &unit) {
    return callManager(bus, StartUnitCall, nullptr, "ss", unit.c_str(), "fail") >= 0;
}
std::optional<PidfdUnit> managerUnitByPidfd(sd_bus *bus, int pidfd) {
    sd_bus_message *raw = nullptr;
    if (callManager(bus, GetUnitByPidfdCall, &raw, "h", pidfd) < 0) return std::nullopt;
    Message reply(raw, sd_bus_message_unref);
    const char *path = nullptr, *id = nullptr;
    const void *generation = nullptr;
    std::size_t length = 0;
    if (sd_bus_message_read(raw, "os", &path, &id) < 0
        || sd_bus_message_read_array(raw, 'y', &generation, &length) < 0 || length != 16) return std::nullopt;
    PidfdUnit unit;
    unit.path = path;
    unit.id = id;
    std::memcpy(unit.invocation.data(), generation, 16);
    return unit;
}
}
