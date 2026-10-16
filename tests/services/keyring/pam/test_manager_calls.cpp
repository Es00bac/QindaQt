// SPDX-License-Identifier: GPL-3.0-or-later
// Pins the exact D-Bus messages the keyring PAM bridge sends to the systemd
// manager, and proves the system bus policy admits them for an unprivileged
// caller (lock-screen keyring unlock runs as the user).
//
// A private dbus-daemon loads the REAL installed org.freedesktop.systemd1.conf
// on top of the system bus's default policy (copied from system.conf). A fake
// org.freedesktop.systemd1 records every message it receives. The production
// helpers in manager_calls.cpp send to it, and the test asserts destination,
// path, interface, member, signature and arguments. Two controls keep the test
// honest: the old call (bus name used as the interface) must be rejected by the
// bus, and a bus without the systemd policy must reject even the right call.
//
// Exit status 77 (ctest SKIP) when the policy file or dbus-daemon is missing.
#include "manager_calls.h"
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
using namespace qindaqt::keyring::pambridge;
namespace {
int cases = 0;
// A failed check throws so the PrivateBus/FakeManager destructors still stop the
// daemon and remove its directory before the test reports failure.
struct CheckFailed { const char *what; };
void check(bool value, const char *what) {
    ++cases;
    if (!value) throw CheckFailed{what};
}
constexpr const char *UnitPath = "/org/freedesktop/systemd1/unit/qindaqt_2dkeyring_401000_2eservice";
constexpr const char *JobPath = "/org/freedesktop/systemd1/job/77";

// The system bus default policy, as shipped in /usr/share/dbus-1/system.conf.
constexpr const char *BaseSystemPolicy = R"(
  <policy context="default">
    <allow user="*"/>
    <deny own="*"/>
    <deny send_type="method_call"/>
    <allow send_type="signal"/>
    <allow send_requested_reply="true" send_type="method_return"/>
    <allow send_requested_reply="true" send_type="error"/>
    <allow receive_type="method_call"/>
    <allow receive_type="method_return"/>
    <allow receive_type="error"/>
    <allow receive_type="signal"/>
    <allow send_destination="org.freedesktop.DBus" send_interface="org.freedesktop.DBus"/>
    <allow send_destination="org.freedesktop.DBus" send_interface="org.freedesktop.DBus.Introspectable"/>
    <allow send_destination="org.freedesktop.DBus" send_interface="org.freedesktop.DBus.Properties"/>
    <deny send_destination="org.freedesktop.DBus" send_interface="org.freedesktop.DBus" send_member="UpdateActivationEnvironment"/>
    <deny send_destination="org.freedesktop.DBus" send_interface="org.freedesktop.DBus.Debug.Stats"/>
    <deny send_destination="org.freedesktop.DBus" send_interface="org.freedesktop.systemd1.Activator"/>
  </policy>)";

class PrivateBus {
public:
    // withSystemdPolicy=false is the control: the base policy alone denies every method call.
    bool start(bool withSystemdPolicy) {
        char pattern[] = "/tmp/qindaqt-systemd1-XXXXXX";
        if (!mkdtemp(pattern)) return false;
        m_dir = pattern;
        const passwd *account = getpwuid(geteuid());
        if (!account) return false;
        const std::string socket = m_dir + "/bus";
        std::string config = "<!DOCTYPE busconfig PUBLIC \"-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN\" "
                             "\"http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd\">\n<busconfig>\n"
                             "<type>system</type>\n<listen>unix:path=" + socket + "</listen>\n<auth>EXTERNAL</auth>\n";
        config += BaseSystemPolicy;
        if (withSystemdPolicy) config += std::string("\n<include>") + QINDAQT_SYSTEMD1_BUS_POLICY + "</include>\n";
        // Test-only: lets the unprivileged test user own the name systemd owns (as root) in production.
        config += std::string("<policy user=\"") + account->pw_name + "\"><allow own=\"org.freedesktop.systemd1\"/></policy>\n</busconfig>\n";
        const std::string file = m_dir + "/bus.conf";
        std::ofstream(file) << config;
        int output[2];
        if (pipe(output) != 0) return false;
        m_pid = fork();
        if (m_pid < 0) return false;
        if (m_pid == 0) {
            prctl(PR_SET_PDEATHSIG, SIGTERM); // Never outlive a crashed test.
            dup2(output[1], STDOUT_FILENO);
            close(output[0]); close(output[1]);
            execl(QINDAQT_DBUS_DAEMON, "dbus-daemon", ("--config-file=" + file).c_str(), "--nofork", "--print-address=1", nullptr);
            _exit(127);
        }
        close(output[1]);
        std::string line;
        pollfd wait{output[0], POLLIN, 0};
        while (line.find('\n') == std::string::npos && poll(&wait, 1, 5000) > 0) {
            char buffer[256];
            const ssize_t count = read(output[0], buffer, sizeof(buffer));
            if (count <= 0) break;
            line.append(buffer, static_cast<std::size_t>(count));
        }
        close(output[0]);
        if (line.find('\n') == std::string::npos) return false;
        address = line.substr(0, line.find('\n'));
        return true;
    }
    ~PrivateBus() {
        if (m_pid > 0) { kill(m_pid, SIGTERM); int status = 0; waitpid(m_pid, &status, 0); }
        if (!m_dir.empty()) std::filesystem::remove_all(m_dir);
    }
    std::string address;
private:
    std::string m_dir;
    pid_t m_pid = -1;
};

sd_bus *connectTo(const std::string &address) {
    sd_bus *bus = nullptr;
    if (sd_bus_new(&bus) < 0) return nullptr;
    if (sd_bus_set_address(bus, address.c_str()) < 0 || sd_bus_set_bus_client(bus, 1) < 0
        || sd_bus_set_method_call_timeout(bus, 5000000) < 0 || sd_bus_start(bus) < 0) {
        sd_bus_unref(bus);
        return nullptr;
    }
    return bus;
}

struct Received {
    std::string destination, path, interface, member, signature, first, second;
    bool fdValid = false;
};

// A stand-in for the root system manager, owning org.freedesktop.systemd1.
class FakeManager {
public:
    bool start(const std::string &address) {
        m_bus = connectTo(address);
        if (!m_bus || sd_bus_request_name(m_bus, SystemdBusName, 0) < 0) return false;
        if (sd_bus_add_filter(m_bus, nullptr, &FakeManager::filter, this) < 0) return false;
        m_thread = std::thread([this] {
            while (!m_stop) {
                while (sd_bus_process(m_bus, nullptr) > 0) {}
                sd_bus_wait(m_bus, 50000);
            }
        });
        return true;
    }
    ~FakeManager() {
        m_stop = true;
        if (m_thread.joinable()) m_thread.join();
        if (m_bus) { sd_bus_flush_close_unref(m_bus); }
    }
    std::vector<Received> received() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_received;
    }
    std::atomic<bool> unitLoaded{true};
private:
    static int filter(sd_bus_message *message, void *userdata, sd_bus_error *) {
        auto *self = static_cast<FakeManager *>(userdata);
        uint8_t type = 0;
        if (sd_bus_message_get_type(message, &type) < 0 || type != SD_BUS_MESSAGE_METHOD_CALL) return 0;
        const char *member = sd_bus_message_get_member(message);
        if (!member || !sd_bus_message_get_interface(message)) return 0;
        const std::string interface = sd_bus_message_get_interface(message);
        if (interface.rfind("org.freedesktop.DBus.", 0) == 0) return 0; // Peer/Introspectable noise
        Received entry;
        entry.destination = sd_bus_message_get_destination(message) ? sd_bus_message_get_destination(message) : "";
        entry.path = sd_bus_message_get_path(message) ? sd_bus_message_get_path(message) : "";
        entry.interface = interface;
        entry.member = member;
        entry.signature = sd_bus_message_get_signature(message, 1) ? sd_bus_message_get_signature(message, 1) : "";
        const char *a = nullptr, *b = nullptr;
        if (entry.signature == "s") { sd_bus_message_read(message, "s", &a); }
        else if (entry.signature == "ss") { sd_bus_message_read(message, "ss", &a, &b); }
        else if (entry.signature == "h") {
            int descriptor = -1;
            struct stat information{};
            entry.fdValid = sd_bus_message_read(message, "h", &descriptor) >= 0 && descriptor >= 0
                && fstat(descriptor, &information) == 0 && S_ISCHR(information.st_mode);
        }
        entry.first = a ? a : "";
        entry.second = b ? b : "";
        { std::lock_guard<std::mutex> lock(self->m_mutex); self->m_received.push_back(entry); }
        const std::string name = entry.member;
        int status;
        if (name == "GetUnit" && !self->unitLoaded)
            status = sd_bus_reply_method_errorf(message, "org.freedesktop.systemd1.NoSuchUnit", "Unit %s not loaded.", entry.first.c_str());
        else if (name == "GetUnit" || name == "LoadUnit") status = sd_bus_reply_method_return(message, "o", UnitPath);
        else if (name == "StartUnit") status = sd_bus_reply_method_return(message, "o", JobPath);
        else if (name == "GetUnitByPIDFD") {
            const unsigned char invocation[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
            // sd_bus_message_append wants array ELEMENTS as varargs; a byte array
            // from memory goes through sd_bus_message_append_array.
            sd_bus_message *reply = nullptr;
            status = sd_bus_message_new_method_return(message, &reply);
            if (status >= 0) status = sd_bus_message_append(reply, "os", UnitPath, "qindaqt-keyring@1000.service");
            if (status >= 0) status = sd_bus_message_append_array(reply, 'y', invocation, sizeof(invocation));
            if (status >= 0) status = sd_bus_send(nullptr, reply, nullptr);
            sd_bus_message_unref(reply);
        } else status = sd_bus_reply_method_errorf(message, SD_BUS_ERROR_UNKNOWN_METHOD, "unexpected %s", member);
        return status < 0 ? status : 1; // Handled: sd-bus must not add its own error reply.
    }
    sd_bus *m_bus = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::mutex m_mutex;
    std::vector<Received> m_received;
};

void expectCall(const Received &entry, const char *member, const char *signature) {
    check(entry.destination == "org.freedesktop.systemd1", "destination is the systemd bus name");
    check(entry.path == "/org/freedesktop/systemd1", "path is the manager object");
    check(entry.interface == "org.freedesktop.systemd1.Manager", "interface is org.freedesktop.systemd1.Manager");
    check(entry.member == member, "member");
    check(entry.signature == signature, "signature");
}
}
int run() {
    // The compile-time contract: the interface is never the bus name.
    for (const ManagerCall *call : {&GetUnitCall, &LoadUnitCall, &StartUnitCall, &GetUnitByPidfdCall}) {
        check(std::string(call->destination) == "org.freedesktop.systemd1", "constant destination");
        check(std::string(call->path) == "/org/freedesktop/systemd1", "constant path");
        check(std::string(call->interface) == "org.freedesktop.systemd1.Manager", "constant interface");
        check(std::string(call->interface) != std::string(call->destination), "interface differs from the bus name");
    }
    check(std::string(GetUnitCall.member) == "GetUnit" && std::string(LoadUnitCall.member) == "LoadUnit"
          && std::string(StartUnitCall.member) == "StartUnit" && std::string(GetUnitByPidfdCall.member) == "GetUnitByPIDFD",
          "constant members");
    if (std::string(QINDAQT_SYSTEMD1_BUS_POLICY).empty() || !std::filesystem::exists(QINDAQT_SYSTEMD1_BUS_POLICY)) {
        std::printf("SKIP: %s is not installed (cannot check the real system bus policy)\n", QINDAQT_SYSTEMD1_BUS_POLICY);
        return 77;
    }
    PrivateBus bus;
    if (!bus.start(true)) { std::printf("SKIP: could not start a private dbus-daemon\n"); return 77; }
    FakeManager manager;
    check(manager.start(bus.address), "fake systemd manager owns org.freedesktop.systemd1");
    sd_bus *client = connectTo(bus.address);
    check(client != nullptr, "client connects to the private bus");

    // GetUnit (what the unprivileged lock worker sends).
    const std::string unit = "qindaqt-keyring@1000.service";
    check(managerUnitPath(client, unit, false) == UnitPath, "GetUnit returns the unit path");
    auto seen = manager.received();
    check(seen.size() == 1, "one message received for GetUnit");
    expectCall(seen[0], "GetUnit", "s");
    check(seen[0].first == unit, "GetUnit names the owner unit");
    // LoadUnit (what root login sends).
    check(managerUnitPath(client, unit, true) == UnitPath, "LoadUnit returns the unit path");
    seen = manager.received();
    check(seen.size() == 2, "one message received for LoadUnit");
    expectCall(seen[1], "LoadUnit", "s");
    check(seen[1].first == unit, "LoadUnit names the owner unit");
    // StartUnit (root only), job mode fail.
    check(managerStartUnit(client, unit), "StartUnit succeeds");
    seen = manager.received();
    check(seen.size() == 3, "one message received for StartUnit");
    expectCall(seen[2], "StartUnit", "ss");
    check(seen[2].first == unit && seen[2].second == "fail", "StartUnit uses job mode fail");
    // GetUnitByPIDFD with a real descriptor.
    const int descriptor = open("/dev/null", O_RDONLY | O_CLOEXEC);
    check(descriptor >= 0, "open a descriptor to pass");
    const auto owner = managerUnitByPidfd(client, descriptor);
    close(descriptor);
    check(owner.has_value(), "GetUnitByPIDFD reply parses");
    check(owner->path == UnitPath && owner->id == unit && owner->invocation[0] == 1 && owner->invocation[15] == 16,
          "GetUnitByPIDFD reply fields");
    seen = manager.received();
    check(seen.size() == 4, "one message received for GetUnitByPIDFD");
    expectCall(seen[3], "GetUnitByPIDFD", "h");
    check(seen[3].fdValid, "the pidfd travels as a unix descriptor");

    // An unloaded unit is a clean failure (systemd answers NoSuchUnit), not a crash.
    manager.unitLoaded = false;
    check(managerUnitPath(client, unit, false).empty(), "an unloaded unit yields an empty path");
    manager.unitLoaded = true;

    if (geteuid() != 0) {
        // Control 1: the old call, with the bus name as the interface, is what the
        // system journal showed being rejected. The default deny must catch it
        // and nothing may reach the manager.
        const std::size_t before = manager.received().size();
        sd_bus_error error = SD_BUS_ERROR_NULL;
        sd_bus_message *reply = nullptr;
        const int status = sd_bus_call_method(client, "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                                              "org.freedesktop.systemd1", "GetUnit", &error, &reply, "s", unit.c_str());
        check(status < 0 && sd_bus_error_has_name(&error, SD_BUS_ERROR_ACCESS_DENIED),
              "the wrong interface is rejected by the system bus policy");
        sd_bus_error_free(&error);
        check(manager.received().size() == before, "the rejected call never reached the manager");
    } else {
        std::printf("note: running as root; skipping the unprivileged rejection control\n");
    }
    sd_bus_flush_close_unref(client);

    if (geteuid() != 0) {
        // Control 2: without the systemd policy the base default denies the correct
        // call too, so the acceptance above really comes from the real policy file.
        PrivateBus bare;
        check(bare.start(false), "second private bus starts");
        FakeManager bareManager;
        check(bareManager.start(bare.address), "second fake manager owns the name");
        sd_bus *bareClient = connectTo(bare.address);
        check(bareClient != nullptr, "client connects to the second bus");
        check(managerUnitPath(bareClient, unit, false).empty(), "the base policy alone rejects the correct GetUnit");
        check(bareManager.received().empty(), "nothing reached the second manager");
        sd_bus_flush_close_unref(bareClient);
    }
    std::printf("%d systemd manager message checks passed\n", cases);
    return 0;
}
int main() {
    try {
        return run();
    } catch (const CheckFailed &failure) {
        std::fprintf(stderr, "FAILED after %d checks: %s\n", cases, failure.what);
        return 1;
    }
}
