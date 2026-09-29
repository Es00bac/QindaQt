// SPDX-License-Identifier: GPL-3.0-or-later
#include "bridge.h"
#include <security/pam_modules.h>
#include <pwd.h>
#include <unistd.h>
#include <cstring>
#include <memory>
#include <array>
#include <optional>
#include <algorithm>
namespace qindaqt::keyring::pambridge {
namespace {
constexpr auto Data="qindaqt-keyring-token-v1";
struct State { Identity identity;SecureBuffer login,old; };
void cleanup(pam_handle_t *,void *value,int) { delete static_cast<State *>(value); }
std::optional<Identity> identity(pam_handle_t *pamh) {
    const void *item=nullptr;
    if (pam_get_item(pamh,PAM_USER,&item)!=PAM_SUCCESS || !item || strnlen(static_cast<const char *>(item),257)>256) return {};
    std::array<char,16384> buffer{};passwd result{},*found=nullptr;
    if (getpwnam_r(static_cast<const char *>(item),&result,buffer.data(),buffer.size(),&found)!=0 || !found
        || (geteuid()!=0 && result.pw_uid!=geteuid())) return {};
    return Identity{result.pw_uid,result.pw_gid};
}
State *state(pam_handle_t *pamh,bool create) {
    const void *data=nullptr;pam_get_data(pamh,Data,&data);
    auto *existing=const_cast<State *>(static_cast<const State *>(data));
    const auto fixed=identity(pamh);
    if (existing && (!fixed || existing->identity.uid!=fixed->uid || existing->identity.gid!=fixed->gid)) {
        pam_set_data(pamh,Data,nullptr,cleanup);return nullptr;
    }
    if (!fixed) return nullptr;
    if (!existing && create) {
        auto value=std::make_unique<State>();value->identity=*fixed;
        existing=value.get();
        if (pam_set_data(pamh,Data,value.release(),cleanup)!=PAM_SUCCESS) { delete existing;return nullptr; }
    }
    return existing;
}
SecureBuffer token(pam_handle_t *pamh,int item) {
    const void *borrowed=nullptr;
    if (pam_get_item(pamh,item,&borrowed)!=PAM_SUCCESS || !borrowed) return SecureBuffer();
    const auto *text=static_cast<const char *>(borrowed);const auto length=strnlen(text,4097);
    if (length==0 || length>4096) return SecureBuffer();
    SecureBuffer owned(length);
    std::memcpy(owned.bytes().data(),text,length);
    // AGENT-CONTRACT: PAM owns borrowed bytes. Never clear/free that pointer;
    // owned token pages are retired on consumption/replacement/pam_end.
    return owned;
}
Options options(int argc,const char **argv) {
    Options value;
#ifdef QINDAQT_PAM_FIXTURE
    for (int i=0;i<argc;++i) {
        const std::string argument(argv[i]);
        if (argument.starts_with("runtime=")) value.runtime=argument.substr(8);
        if (argument.starts_with("owner=")) value.fixtureOwner=static_cast<pid_t>(std::stol(argument.substr(6)));
    }
#else
    (void)argc;(void)argv;
#endif
    return value;
}
}
}
extern "C" int pam_sm_authenticate(pam_handle_t *pamh,int,int,const char **) {
    using namespace qindaqt::keyring::pambridge;
    try { if (auto *value=state(pamh,true)) {
        value->login.clear();value->login=token(pamh,PAM_AUTHTOK);
    } } catch (...) {}
    // This observer does not authenticate login and must never grant access.
    return PAM_IGNORE;
}
extern "C" int pam_sm_setcred(pam_handle_t *,int,int,const char **) { return PAM_IGNORE; }
extern "C" int pam_sm_open_session(pam_handle_t *pamh,int,int argc,const char **argv) {
    using namespace qindaqt::keyring::pambridge;
    try {
        if (auto *value=state(pamh,false)) {
            auto password=std::move(value->login);
            if (password.size()) bridge(value->identity,options(argc,argv),1,password.bytes(),{});
        }
    } catch (...) {}
    return PAM_SUCCESS; // Keyring availability/mismatch must never prevent login.
}
extern "C" int pam_sm_close_session(pam_handle_t *pamh,int,int,const char **) {
    using namespace qindaqt::keyring::pambridge;
    pam_set_data(pamh,Data,nullptr,cleanup);return PAM_SUCCESS;
}
extern "C" int pam_sm_chauthtok(pam_handle_t *pamh,int flags,int argc,const char **argv) {
    using namespace qindaqt::keyring::pambridge;
    try {
        auto *value=state(pamh,true);if (!value) return PAM_AUTHTOK_RECOVERY_ERR;
        if (flags & PAM_PRELIM_CHECK) {
            value->old.clear();value->old=token(pamh,PAM_OLDAUTHTOK);return PAM_SUCCESS;
        }
        if (!(flags & PAM_UPDATE_AUTHTOK)) return PAM_AUTHTOK_ERR;
        auto old=token(pamh,PAM_OLDAUTHTOK);
        if (!old.size()) old=std::move(value->old);else value->old.clear();
        auto changed=token(pamh,PAM_AUTHTOK);
        value->login.clear();
        if (!old.size() || !changed.size()) return PAM_AUTHTOK_RECOVERY_ERR;
        return bridge(value->identity,options(argc,argv),2,old.bytes(),changed.bytes()) ? PAM_SUCCESS : PAM_AUTHTOK_ERR;
    } catch (...) { return PAM_AUTHTOK_ERR; }
}
