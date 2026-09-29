// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include <pwd.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <array>
#include <string>
#include <vector>
#include <cstring>
int main(int argc,char **argv) {
    using namespace qindaqt::keyring::pambridge;
    if (!processProtection()) return 2;
    const auto uid=std::to_string(geteuid());
#ifndef QINDAQT_PAM_FIXTURE
    if (argc!=3 || std::string(argv[1])!="--uid" || uid!=argv[2]
        || !protectedFile(QINDAQT_KEYRING_DAEMON,0,true)) return 2;
    std::array<char,16384> account{};passwd entry{},*found=nullptr;
    if (getpwuid_r(geteuid(),&entry,account.data(),account.size(),&found)!=0 || !found
        || !entry.pw_dir || entry.pw_dir[0]!='/') return 2;
    const std::string runtime="/run/user/"+uid;
    if (!privateRuntime(runtime,geteuid())) return 2;
    std::vector<std::string> variables{"PATH=/usr/bin:/bin","LANG=C.UTF-8","HOME="+std::string(entry.pw_dir),
        "XDG_RUNTIME_DIR="+runtime,"XDG_DATA_HOME="+std::string(entry.pw_dir)+"/.local/share",
        "DBUS_SESSION_BUS_ADDRESS=unix:path="+runtime+"/bus","WAYLAND_DISPLAY=wayland-0","QT_QPA_PLATFORM=wayland"};
    std::vector<char *> environment;for (auto &value:variables) environment.push_back(value.data());environment.push_back(nullptr);
    char *arguments[]{const_cast<char *>(QINDAQT_KEYRING_DAEMON),nullptr};
#else
    if (argc!=7 || !protectedFile(QINDAQT_KEYRING_DAEMON,geteuid(),true)) return 2;
    std::vector<std::string> variables{"PATH=/usr/bin:/bin","LANG=C.UTF-8",
        "DBUS_SESSION_BUS_ADDRESS="+std::string(argv[2]),"XDG_RUNTIME_DIR="+std::string(argv[3])};
    std::vector<char *> environment;for (auto &value:variables) environment.push_back(value.data());environment.push_back(nullptr);
    char *arguments[]{const_cast<char *>(QINDAQT_KEYRING_DAEMON),const_cast<char *>("--private-bus"),argv[2],
        const_cast<char *>("--runtime-root"),argv[4],const_cast<char *>("--storage-root"),argv[5],
        const_cast<char *>("--prompt-program"),argv[6],nullptr};
#endif
    if (syscall(SYS_close_range,3U,~0U,0U)!=0) return 2;
    execve(QINDAQT_KEYRING_DAEMON,arguments,environment.data());return 2;
}
