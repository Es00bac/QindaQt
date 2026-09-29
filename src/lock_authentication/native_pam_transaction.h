// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "authentication.h"
#include <security/pam_appl.h>

namespace QindaQt::LockAuthentication {
class NativePamTransaction final : public PamTransaction {
public:
  NativePamTransaction() = default;
#if defined(QINDAQT_PRIVATE_PAM_FIXTURE)
  // Compiled only into the non-installed fixture executable, never the worker.
  explicit NativePamTransaction(std::string fixtureConfiguration);
#endif
  ~NativePamTransaction() override;
  bool start(std::string_view service, std::string_view user,
             Conversation &conversation) override;
  bool authenticate() override;
  bool approveAccount() override;

private:
  static int converse(int count, const pam_message **messages,
                      pam_response **responses, void *context);
  Conversation *m_conversation =
      nullptr; // Borrowed until pam_end; one worker attempt.
  pam_handle_t *m_handle = nullptr;
  int m_status = PAM_SYSTEM_ERR;
  std::size_t m_messages = 0;
#if defined(QINDAQT_PRIVATE_PAM_FIXTURE)
  std::string m_fixtureConfiguration;
#endif
};
} // namespace QindaQt::LockAuthentication
