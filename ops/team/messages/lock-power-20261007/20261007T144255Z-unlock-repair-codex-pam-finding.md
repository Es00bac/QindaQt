# Native locker PAM service omission

- Timestamp: 2026-10-07T14:42:55.628370+00:00
- Base: 46e6a74dc0de6b279ca8c2a24e50d3f86634d925
- Source: src/lock_authentication/authentication.cpp calls pam_start with fixed qindaqt-lock via NativePamTransaction. Main.qml displays password only while authentication.waiting, set only by Secret/Visible messages from owned PAM worker.
- Read-only qinda evidence: /etc/pam.d/qindaqt-lock, /usr/lib/pam.d/qindaqt-lock and /usr/lib64/pam.d/qindaqt-lock absent; installed r13 CONTENTS has lock/worker executables but no PAM service file. /etc/pam.d/other requires pam_deny.so for auth/account/password/session. Manager separately confirms same laptop omission.

The existing October 4 production namespace gate masked PAM and qualified only launch, standard role and software frame; it cannot qualify a password prompt. Missing named service with deny-only other fallback returns Denied without a conversation. A focused private-confdir worker-channel reproduction is being built; no host PAM/authentication/credential use.

Manager owns proposed sys-auth/qindaqt-lock-pam package and desktop runtime dependency. Stack proposal uses distro system-auth for auth/account and optional native keyring-only session notification; it does not create a second logind session. Native process protection does not set NoNewPrivileges; live parent/helper mode verification remains manager-owned. Actual owner password/physical unlock remains unqualified until owner-controlled runtime evidence.
