// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <qindaqt/services/keyring/secure_buffer.h>
#include <sys/types.h>
#include <span>
#include <string>
namespace qindaqt::keyring::pambridge {
// One synchronous exchange per helper; no shared state across PAM handles.
// Token spans are borrowed until return; copies use SecureBuffer. Failure
// never acknowledges persistence. No exceptions cross exported PAM functions.
struct Identity { uid_t uid; gid_t gid; };
struct Options {
    std::string runtime;
#ifdef QINDAQT_PAM_FIXTURE
    pid_t fixtureOwner = 0;
#endif
};
struct Fd {
    int value = -1;
    explicit Fd(int n = -1) : value(n) {}
    ~Fd();
    Fd(const Fd &) = delete;
    Fd &operator=(const Fd &) = delete;
};
bool protectedFile(const std::string &,uid_t owner,bool executable);
// Returns an owned descriptor or -1; caller closes it. Traversal is no-follow.
int openPrivateRuntime(const std::string &,uid_t owner);
bool privateRuntime(const std::string &,uid_t owner);
bool ptracePolicySupported();
bool processProtection();
bool exchange(int fd,std::span<const unsigned char> outgoing,
              std::span<unsigned char> incoming,int milliseconds);
bool startOwner(uid_t uid);
bool verifyOwner(uid_t uid,pid_t peer,int pidfd,const Options &);
bool bridge(Identity,const Options &,unsigned char operation,
            std::span<const unsigned char> oldToken,std::span<const unsigned char> newToken);
}
