// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include "owner_policy.h"
#include <poll.h>
// Test-only manager seam pins the sanitized child created by the harness.
// Production has no runtime owner= option and uses system GetUnitByPIDFD.
namespace qindaqt::keyring::pambridge {
bool startOwner(uid_t) { return true; }
bool verifyOwner(uid_t uid,pid_t peer,int pidfd,const Options &options) {
    pollfd alive{pidfd,POLLIN,0};
    OwnerIdentity owner{ownerUnit(uid),std::to_string(uid),"active","running",
        static_cast<std::uint32_t>(options.fixtureOwner),false,false,false};
    return runningOwner(owner,uid,peer) && poll(&alive,1,0)==0;
}
}
