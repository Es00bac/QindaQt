// SPDX-License-Identifier: GPL-3.0-or-later
#include "authentication.h"

namespace QindaQt::LockAuthentication {
Outcome authenticateSessionUser(PamTransaction &pam, Conversation &conversation,
                                std::string_view sessionUser) {
  if (conversation.cancelled()) {
    return Outcome::Cancelled;
  }
  if (sessionUser.empty() || sessionUser.size() > 256 ||
      sessionUser.find('\0') != std::string_view::npos ||
      !pam.start("qindaqt-lock", sessionUser, conversation)) {
    return conversation.cancelled() ? Outcome::Cancelled : Outcome::Unavailable;
  }
  if (!pam.authenticate()) {
    return conversation.cancelled() ? Outcome::Cancelled : Outcome::Denied;
  }
  if (conversation.cancelled()) {
    return Outcome::Cancelled;
  }
  if (!pam.approveAccount()) {
    return conversation.cancelled() ? Outcome::Cancelled
                                    : Outcome::AccountDenied;
  }
  return conversation.cancelled() ? Outcome::Cancelled : Outcome::Authenticated;
}
} // namespace QindaQt::LockAuthentication
