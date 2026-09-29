// SPDX-License-Identifier: GPL-3.0-or-later
#include "native_pam_transaction.h"
#include <cstdlib>
#include <cstring>
#include <string>

namespace QindaQt::LockAuthentication {
#if defined(QINDAQT_PRIVATE_PAM_FIXTURE)
NativePamTransaction::NativePamTransaction(std::string fixtureConfiguration)
    : m_fixtureConfiguration(std::move(fixtureConfiguration)) {}
#endif
NativePamTransaction::~NativePamTransaction() {
  if (m_handle) {
    pam_end(m_handle, m_status);
  }
}
bool NativePamTransaction::start(std::string_view service,
                                 std::string_view user,
                                 Conversation &conversation) {
  if (m_handle) {
    return false;
  }
  m_conversation = &conversation;
  const pam_conv conv{&NativePamTransaction::converse, this};
  const std::string serviceName(service), userName(user);
#if defined(QINDAQT_PRIVATE_PAM_FIXTURE)
  if (!m_fixtureConfiguration.empty()) {
    m_status = pam_start_confdir(serviceName.c_str(), userName.c_str(), &conv,
                                 m_fixtureConfiguration.c_str(), &m_handle);
  } else
#endif
  {
    m_status =
        pam_start(serviceName.c_str(), userName.c_str(), &conv, &m_handle);
  }
  if (m_status != PAM_SUCCESS) {
    return false;
  }
  m_status = pam_set_item(m_handle, PAM_TTY, "qindaqt-lock");
  return m_status == PAM_SUCCESS;
}
bool NativePamTransaction::authenticate() {
  if (!m_handle) {
    return false;
  }
  m_status = pam_authenticate(m_handle, PAM_DISALLOW_NULL_AUTHTOK);
  return m_status == PAM_SUCCESS;
}
bool NativePamTransaction::approveAccount() {
  if (!m_handle) {
    return false;
  }
  m_status = pam_acct_mgmt(m_handle, 0);
  // Expired credentials require a separate sign-in/password-change flow;
  // PAM_NEW_AUTHTOK_REQD is never an authenticated unlock result.
  return m_status == PAM_SUCCESS;
}
int NativePamTransaction::converse(int count, const pam_message **messages,
                                   pam_response **responses, void *context) {
  auto *self = static_cast<NativePamTransaction *>(context);
  if (!self || !self->m_conversation || !responses || !messages || count <= 0 ||
      std::size_t(count) > maximumMessages - self->m_messages ||
      self->m_conversation->cancelled()) {
    return PAM_CONV_ERR;
  }
  self->m_messages += static_cast<std::size_t>(count);
  auto *result =
      static_cast<pam_response *>(calloc(static_cast<std::size_t>(count), sizeof(pam_response)));
  if (!result) {
    return PAM_BUF_ERR;
  }
  auto reject = [result, count]() {
    for (int i = 0; i < count; ++i) {
      if (result[i].resp) {
        explicit_bzero(result[i].resp, strlen(result[i].resp));
        free(result[i].resp);
      }
    }
    free(result);
    return PAM_CONV_ERR;
  };
  for (int i = 0; i < count; ++i) {
    if (!messages[i] || !messages[i]->msg ||
        strnlen(messages[i]->msg, maximumPromptBytes + 1) >
            maximumPromptBytes) {
      return reject();
    }
    MessageKind kind;
    switch (messages[i]->msg_style) {
    case PAM_PROMPT_ECHO_OFF:
      kind = MessageKind::Secret;
      break;
    case PAM_PROMPT_ECHO_ON:
      kind = MessageKind::Visible;
      break;
    case PAM_TEXT_INFO:
      kind = MessageKind::Information;
      break;
    case PAM_ERROR_MSG:
      kind = MessageKind::Error;
      break;
    default:
      return reject();
    }
    auto reply = self->m_conversation->exchange(kind, messages[i]->msg);
    if (!reply) {
      return reject();
    }
    const bool prompt =
        kind == MessageKind::Secret || kind == MessageKind::Visible;
    if (reply->size() > maximumResponseBytes ||
        reply->find('\0') != std::string::npos ||
        self->m_conversation->cancelled()) {
      explicit_bzero(reply->data(), reply->size());
      return reject();
    }
    if (prompt) {
      result[i].resp = static_cast<char *>(malloc(reply->size() + 1));
      if (!result[i].resp) {
        explicit_bzero(reply->data(), reply->size());
        return reject();
      }
      memcpy(result[i].resp, reply->data(), reply->size());
      result[i].resp[reply->size()] = '\0';
    }
    explicit_bzero(reply->data(), reply->size());
  }
  // libpam owns successful response allocations. Our disposable worker exits
  // after this attempt; responses never enter resident UI state or logs.
  *responses = result;
  return PAM_SUCCESS;
}
} // namespace QindaQt::LockAuthentication
