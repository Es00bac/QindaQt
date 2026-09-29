// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include "owner_policy.h"
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
namespace { int cases=0;void check(bool value) { ++cases;if (!value) std::abort(); } }
int main() {
    using namespace qindaqt::keyring::pambridge;
    const auto uid=geteuid();
    OwnerIdentity trusted{ownerUnit(uid),std::to_string(uid),"active","running",42,false,false,false};
    check(configuredOwner(trusted,uid));check(runningOwner(trusted,uid,42));
    check(!runningOwner(trusted,uid,43));check(!runningOwner(trusted,uid,1));
    for (int mutation=0;mutation<8;++mutation) {
        auto bad=trusted;
        switch (mutation) {
        case 0:bad.unit="qindaqt-keyring@other.service";break;
        case 1:bad.user="other";break;
        case 2:bad.transient=true;break;
        case 3:bad.delegated=true;break;
        case 4:bad.dropIns=true;break;
        case 5:bad.active="activating";break;
        case 6:bad.substate="dead";break;
        case 7:bad.mainPid=0;break;
        }
        check(!runningOwner(bad,uid,42));
    }
    const auto root=std::filesystem::current_path()/"build/pk3/owner-policy-fixture";
    std::filesystem::remove_all(root);std::filesystem::create_directories(root);chmod(root.c_str(),0700);
    const auto file=root/"program";std::ofstream(file)<<"synthetic";chmod(file.c_str(),0700);
    check(protectedFile(file,uid,true));
    chmod(file.c_str(),0720);check(!protectedFile(file,uid,true));chmod(file.c_str(),0700);
    std::filesystem::create_symlink(file,root/"alias");check(!protectedFile(root/"alias",uid,true));
    std::filesystem::create_directory_symlink(root,root/"redirect");check(!protectedFile(root/"redirect/program",uid,true));
    check(privateRuntime(root,uid));chmod(root.c_str(),0755);check(!privateRuntime(root,uid));
    chmod(root.c_str(),0700);check(!privateRuntime(root/"redirect",uid));
    check(!privateRuntime(root/"..",uid));check(!privateRuntime(root/"missing",uid));
    std::filesystem::remove_all(root);
    std::printf("%d owner/path rejection checks passed\n",cases);return 0;
}
