// SPDX-License-Identifier: GPL-3.0-or-later
// Non-installed libpam conversation witness; never references system PAM config
// or an owner's password. Only the fixture's private pam_start_confdir loads
// it.
#include <cstdlib>
#include <cstring>
#include <security/pam_appl.h>
#include <security/pam_modules.h>
#include <string>
#include <vector>
extern "C" int pam_sm_authenticate(pam_handle_t *handle, int, int argc,
                                   const char **argv) {
  const std::string mode = argc ? argv[0] : "deny";
  const void *item = nullptr;
  if (pam_get_item(handle, PAM_CONV, &item) != PAM_SUCCESS || !item) {
    return PAM_SYSTEM_ERR;
  }
  const auto *conv = static_cast<const pam_conv *>(item);
  const std::string text = mode == "huge-prompt" ? std::string(4097, 'x')
                                                 : "Synthetic fixture password";
  const pam_message prompt{mode == "invalid-style" ? 999 : PAM_PROMPT_ECHO_OFF,
                           text.c_str()};
  std::vector<const pam_message *> messages(mode == "too-many" ? 65 : 1,
                                            &prompt);
  pam_response *response = nullptr;
  const int result = conv->conv(messages.size(), messages.data(), &response,
                                conv->appdata_ptr);
  if (result != PAM_SUCCESS) {
    return PAM_AUTH_ERR;
  }
  bool expected = response && response[0].resp &&
                  !strcmp(response[0].resp, "fixture-response");
  for (std::size_t i = 0; i < messages.size(); ++i) {
    if (response[i].resp) {
      explicit_bzero(response[i].resp, strlen(response[i].resp));
      free(response[i].resp);
    }
  }
  free(response);
  return mode == "deny" || !expected ? PAM_AUTH_ERR : PAM_SUCCESS;
}
extern "C" int pam_sm_setcred(pam_handle_t *, int, int, const char **) {
  return PAM_SUCCESS;
}
extern "C" int pam_sm_acct_mgmt(pam_handle_t *, int, int argc,
                                const char **argv) {
  return argc && !strcmp(argv[0], "account-deny") ? PAM_ACCT_EXPIRED
                                                  : PAM_SUCCESS;
}
