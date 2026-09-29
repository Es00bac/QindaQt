// SPDX-License-Identifier: GPL-3.0-or-later
#include <security/pam_appl.h>
#include <pwd.h>
#include <unistd.h>
#include <cstdio>
#include <sys/resource.h>
#include <cstring>
namespace {
int conversation(int,const pam_message **,pam_response **,void *) { return PAM_CONV_ERR; }
}
int main(int argc,char **argv) {
    if (argc!=4) return 2;
    const auto *account=getpwuid(geteuid());if (!account) return 2;
    pam_conv conv{conversation,nullptr};pam_handle_t *pamh=nullptr;
    int result=pam_start_confdir(argv[2],account->pw_name,&conv,argv[1],&pamh);
    if (result!=PAM_SUCCESS) return 3;
    if (std::strcmp(argv[3],"password")==0) result=pam_chauthtok(pamh,0);
    else {
        result=pam_authenticate(pamh,0);
        if (result==PAM_SUCCESS) {
            if (std::strcmp(argv[3],"retry-memory-failure")==0) {
                rlimit previous{};if (getrlimit(RLIMIT_MEMLOCK,&previous)!=0) return 4;
                const rlimit unavailable{0,previous.rlim_max};
                if (setrlimit(RLIMIT_MEMLOCK,&unavailable)!=0) return 4;
                result=pam_authenticate(pamh,0);
                if (setrlimit(RLIMIT_MEMLOCK,&previous)!=0 || result!=PAM_SUCCESS) return 4;
            }
            if (std::strcmp(argv[3],"changed-user")==0) pam_set_item(pamh,PAM_USER,"qindaqt-no-such-synthetic-user");
            result=pam_open_session(pamh,0);
            if (result==PAM_SUCCESS) pam_close_session(pamh,0);
        }
    }
    pam_end(pamh,result);
    // Credentials are compiled synthetic fixtures; report only status.
    std::printf("%d\n",result);return result==PAM_SUCCESS ? 0 : 1;
}
