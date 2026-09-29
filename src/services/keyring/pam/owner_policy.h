// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <sys/types.h>
#include <cstdint>
#include <string>
namespace qindaqt::keyring::pambridge {
// Production values come from the authenticated system manager. Tests replace
// that authority port; peer identity and liveness still come from the kernel.
struct OwnerIdentity {
    std::string unit,user,active,substate;
    std::uint32_t mainPid=0;
    bool transient=false,delegated=false,dropIns=false;
};
inline std::string ownerUnit(uid_t uid) { return "qindaqt-keyring@"+std::to_string(uid)+".service"; }
inline bool configuredOwner(const OwnerIdentity &v,uid_t uid) {
    return v.unit==ownerUnit(uid) && v.user==std::to_string(uid) && !v.transient && !v.delegated && !v.dropIns;
}
inline bool runningOwner(const OwnerIdentity &v,uid_t uid,pid_t peer) {
    return configuredOwner(v,uid) && peer>1 && v.mainPid==static_cast<std::uint32_t>(peer)
        && v.active=="active" && v.substate=="running";
}
}
