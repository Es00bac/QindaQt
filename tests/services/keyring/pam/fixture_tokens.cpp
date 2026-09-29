// SPDX-License-Identifier: GPL-3.0-or-later
#include <security/pam_modules.h>
#include <string>
#include <cstring>
namespace {
const char *mode(int argc,const char **argv) { return argc>0 ? argv[0] : "good"; }
int assign(pam_handle_t *pamh,int item,const char *value) { return pam_set_item(pamh,item,value); }
}
extern "C" int pam_sm_authenticate(pam_handle_t *pamh,int,int argc,const char **argv) {
    const std::string option=mode(argc,argv);
    if (option=="missing") return PAM_SUCCESS;
    if (option=="oversize") {
        char value[4098];std::memset(value,'x',4097);value[4097]=0;
        const int result=assign(pamh,PAM_AUTHTOK,value);
        explicit_bzero(value,sizeof(value));return result;
    }
    return assign(pamh,PAM_AUTHTOK,option=="wrong" ? "synthetic-wrong" : option=="empty" ? "" : "synthetic-keyring-password");
}
extern "C" int pam_sm_setcred(pam_handle_t *,int,int,const char **) { return PAM_SUCCESS; }
extern "C" int pam_sm_open_session(pam_handle_t *,int,int,const char **) { return PAM_SUCCESS; }
extern "C" int pam_sm_close_session(pam_handle_t *,int,int,const char **) { return PAM_SUCCESS; }
extern "C" int pam_sm_chauthtok(pam_handle_t *pamh,int flags,int argc,const char **argv) {
    const std::string option=mode(argc,argv);
    if (flags & PAM_PRELIM_CHECK)
        return assign(pamh,PAM_OLDAUTHTOK,option=="no-old" ? nullptr : option=="wrong" ? "synthetic-wrong" : "synthetic-keyring-password");
    return assign(pamh,PAM_AUTHTOK,option=="no-new" ? nullptr : "synthetic-changed-password");
}
