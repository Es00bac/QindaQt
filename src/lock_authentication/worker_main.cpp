// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_pam_transaction.h"
#include "worker_conversation.h"
#include <array>
#include <cstdlib>
#include <pwd.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>
using namespace QindaQt::LockAuthentication;
int main(int argc, char **argv) {
  // AGENT-GUARD: exec resets dumpability. Protect before libpam or any secret
  // arrives; launcher must also enforce Yama 1+ for the exec-to-main interval.
  const rlimit noCore{0, 0};
  if (setrlimit(RLIMIT_CORE, &noCore) || prctl(PR_SET_DUMPABLE, 0) ||
      prctl(PR_GET_DUMPABLE) != 0 || getuid() != geteuid() || getgid() != getegid()) return 2;
#if defined(QINDAQT_PRIVATE_PAM_FIXTURE)
  if (argc != 2) return 2;
#else
  (void)argv;
  if (argc != 1) return 2;
#endif
  // Never accept a caller-supplied account/service or fall back to $USER.
  std::array<char, 65536> accountBuffer{};
  passwd account{}; passwd *found = nullptr;
  if (getpwuid_r(getuid(), &account, accountBuffer.data(), accountBuffer.size(), &found) ||
      !found || !found->pw_name) return 2;
  WorkerChannel channel(3, std::chrono::seconds(90));
  auto begin = channel.receive();
  if (!begin || begin->kind != WireKind::Begin || !begin->payload.empty()) return 2;
  WorkerConversation conversation(channel, begin->token);
  Outcome outcome;
  {
#if defined(QINDAQT_PRIVATE_PAM_FIXTURE)
    NativePamTransaction pam(argv[1]); // Non-installed executable only.
#else
    NativePamTransaction pam;
#endif
    outcome = authenticateSessionUser(pam, conversation, found->pw_name);
    if (outcome == Outcome::Authenticated && !conversation.cancelled()) pam.notifyApprovedUnlock();
    // pam_end/module cleanup runs while the conversation/channel still exist,
    // and before reporting approval, never against a destroyed borrowed port.
  }
  if (conversation.cancelled()) outcome = Outcome::Cancelled;
  const char result = static_cast<char>('0' + static_cast<int>(outcome));
  return channel.send({WireKind::Result, begin->token, std::string(1, result)}) ? 0 : 2;
}
