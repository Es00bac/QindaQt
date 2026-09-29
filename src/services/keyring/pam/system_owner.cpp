// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include "owner_policy.h"
#include <systemd/sd-bus.h>
#include <memory>
#include <array>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <unistd.h>
namespace qindaqt::keyring::pambridge {
namespace {
constexpr auto Manager="org.freedesktop.systemd1";
constexpr auto ManagerPath="/org/freedesktop/systemd1";
constexpr auto UnitInterface="org.freedesktop.systemd1.Unit";
constexpr auto ServiceInterface="org.freedesktop.systemd1.Service";
using Bus=std::unique_ptr<sd_bus,decltype(&sd_bus_unref)>;
using Message=std::unique_ptr<sd_bus_message,decltype(&sd_bus_message_unref)>;
Bus openManager() {
    sd_bus *raw=nullptr;
    if (sd_bus_new(&raw)<0) return {nullptr,sd_bus_unref};
    Bus bus(raw,sd_bus_unref);
    // Never honor caller/user PAM environment addresses for system authority.
    if (sd_bus_set_address(raw,"unix:path=/run/dbus/system_bus_socket")<0
        || sd_bus_set_bus_client(raw,1)<0 || sd_bus_set_method_call_timeout(raw,400000)<0 || sd_bus_start(raw)<0)
        return {nullptr,sd_bus_unref};
    sd_bus_creds *credentials=nullptr;
    if (sd_bus_get_name_creds(raw,Manager,SD_BUS_CREDS_EUID | SD_BUS_CREDS_PID,&credentials)<0) return {nullptr,sd_bus_unref};
    uid_t uid=1;pid_t pid=0;
    const bool trusted=sd_bus_creds_get_euid(credentials,&uid)>=0 && uid==0
        && sd_bus_creds_get_pid(credentials,&pid)>=0 && pid==1;
    sd_bus_creds_unref(credentials);
    if (!trusted) return {nullptr,sd_bus_unref};
    return bus;
}
std::string unitName(uid_t uid) { return ownerUnit(uid); }
std::string property(sd_bus *bus,const std::string &path,const char *iface,const char *name) {
    char *raw=nullptr;
    if (sd_bus_get_property_string(bus,Manager,path.c_str(),iface,name,nullptr,&raw)<0) return {};
    std::string result(raw);free(raw);return result;
}
bool emptyArray(sd_bus *bus,const std::string &path,const char *iface,const char *name,const char *signature) {
    sd_bus_message *raw=nullptr;
    if (sd_bus_get_property(bus,Manager,path.c_str(),iface,name,nullptr,&raw,signature)<0) return false;
    Message value(raw,sd_bus_message_unref);
    return sd_bus_message_enter_container(raw,'a',signature+1)>=0 && sd_bus_message_at_end(raw,0)>0;
}
bool boolean(sd_bus *bus,const std::string &path,const char *iface,const char *name,bool expected) {
    int value=0;return sd_bus_get_property_trivial(bus,Manager,path.c_str(),iface,name,nullptr,'b',&value)>=0
        && (value!=0)==expected;
}
bool command(sd_bus *bus,const std::string &path,uid_t uid) {
    sd_bus_message *raw=nullptr;
    if (sd_bus_get_property(bus,Manager,path.c_str(),ServiceInterface,"ExecStart",nullptr,&raw,"a(sasbttttuii)")<0) return false;
    Message value(raw,sd_bus_message_unref);
    const char *program=nullptr,*argument=nullptr;
    if (sd_bus_message_enter_container(raw,'a',"(sasbttttuii)")<0
        || sd_bus_message_enter_container(raw,'r',"sasbttttuii")<=0
        || sd_bus_message_read(raw,"s",&program)<0 || std::string(program)!=QINDAQT_KEYRING_LAUNCHER
        || sd_bus_message_enter_container(raw,'a',"s")<0) return false;
    std::array<std::string,3> expected{QINDAQT_KEYRING_LAUNCHER,"--uid",std::to_string(uid)};
    for (const auto &text : expected)
        if (sd_bus_message_read(raw,"s",&argument)<=0 || text!=argument) return false;
    return sd_bus_message_at_end(raw,0)>0 && sd_bus_message_exit_container(raw)>=0
        && sd_bus_message_skip(raw,"bttttuii")>=0 && sd_bus_message_exit_container(raw)>=0
        && sd_bus_message_at_end(raw,0)>0;
}
OwnerIdentity identity(sd_bus *bus,const std::string &path) {
    OwnerIdentity value;
    value.unit=property(bus,path,UnitInterface,"Id");
    value.user=property(bus,path,ServiceInterface,"User");
    value.active=property(bus,path,UnitInterface,"ActiveState");
    value.substate=property(bus,path,UnitInterface,"SubState");
    value.transient=!boolean(bus,path,UnitInterface,"Transient",false);
    value.delegated=!boolean(bus,path,ServiceInterface,"Delegate",false);
    value.dropIns=!emptyArray(bus,path,UnitInterface,"DropInPaths","as");
    if (sd_bus_get_property_trivial(bus,Manager,path.c_str(),ServiceInterface,"MainPID",nullptr,'u',&value.mainPid)<0)
        value.mainPid=0;
    return value;
}
bool configured(sd_bus *bus,const std::string &path,uid_t uid) {
    const std::string fragment=property(bus,path,UnitInterface,"FragmentPath");
    return configuredOwner(identity(bus,path),uid)
        && fragment==QINDAQT_KEYRING_SYSTEM_UNIT && protectedFile(fragment,0,false)
        && protectedFile(QINDAQT_KEYRING_LAUNCHER,0,true) && protectedFile(QINDAQT_KEYRING_DAEMON,0,true)
        && command(bus,path,uid);
}
std::string load(sd_bus *bus,uid_t uid) {
    sd_bus_message *raw=nullptr;
    if (sd_bus_call_method(bus,Manager,ManagerPath,Manager,geteuid()==0 ? "LoadUnit" : "GetUnit",nullptr,&raw,"s",unitName(uid).c_str())<0) return {};
    Message reply(raw,sd_bus_message_unref);const char *path=nullptr;
    return sd_bus_message_read(raw,"o",&path)>0 ? std::string(path) : std::string();
}
}
bool startOwner(uid_t uid) {
    auto bus=openManager();if (!bus) return false;
    const auto path=load(bus.get(),uid);
    if (path.empty() || !configured(bus.get(),path,uid)) return false;
    const auto owner=identity(bus.get(),path);
    if (owner.mainPid>1 && owner.active=="active" && owner.substate=="running") return true;
    // Lock reauthentication runs as the user. It may use an existing trusted
    // system owner, never trigger a privileged StartUnit/polkit conversation.
    if (geteuid()!=0) return false;
    return sd_bus_call_method(bus.get(),Manager,ManagerPath,Manager,"StartUnit",nullptr,nullptr,
                             "ss",unitName(uid).c_str(),"fail")>=0;
}
bool verifyOwner(uid_t uid,pid_t peer,int pidfd,const Options &) {
    auto bus=openManager();if (!bus) return false;
    sd_bus_message *raw=nullptr;
    if (sd_bus_call_method(bus.get(),Manager,ManagerPath,Manager,"GetUnitByPIDFD",nullptr,&raw,"h",pidfd)<0) return false;
    Message reply(raw,sd_bus_message_unref);const char *path=nullptr,*id=nullptr;const void *generation=nullptr;std::size_t length=0;
    if (sd_bus_message_read(raw,"os",&path,&id)<0 || std::string(id)!=unitName(uid)
        || sd_bus_message_read_array(raw,'y',&generation,&length)<0 || length!=16
        || std::memcmp(generation,std::array<unsigned char,16>{}.data(),16)==0) return false;
    // A helper handles one exchange on one thread. The recheck must retain
    // the same system-manager invocation, never a restarted service.
    static std::array<unsigned char,16> invocation{};
    static bool captured=false;
    if (captured && std::memcmp(invocation.data(),generation,16)!=0) return false;
    if (!captured) { std::memcpy(invocation.data(),generation,16);captured=true; }
    const std::string object(path);
    return configured(bus.get(),object,uid) && runningOwner(identity(bus.get(),object),uid,peer);
}
}
