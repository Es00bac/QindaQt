# Installed lock policy and battery recovery handoff

- Timestamp: 2026-10-07T15:10:28.275875+00:00
- Exact accepted integration: `8538f23643ce04afeb9ee20180c3c4bbe10e41d1` on qinda hub main.
- Accepted source candidates: native missing-service regression `1297b81ad9b832d4fe4d152aa8ecd344950a010d`; bounded UPower activation `6921f4c218550aa6e8f7690a909d3e2d633b603b`.
- Exact overlay: `e62153bc6b1ed54fbda97e63c70bc918e8588bb7`, integrated on master with clean QindaGentoo checkout.
- Installed signed artifact: `5eeb37dbd90bb34c31b56183701c1e84f40692bb33287a5007fddba11d8cc589`, sys-auth/qindaqt-lock-pam-1, one source-identical policy file root:root0644/486bytes/CONTENTS passes on both hosts. Mandatory signature requests used for both binary-only Portage installs; exit0.
- Existing login PAM policies, world/world_sets and all prior installed package versions remain byte-identical/unchanged. Laptop desktop remains unlocked/unprotected in the same session.
- Live laptop UPower active and enabled at graphical boot; normal activation restored Power1 ready, latest kernel battery57% charging. Source activation guard is integrated for the next desktop delivery; installed r13 binary was not replaced.
- Gates on combined tree: strict11-target manager build exit0; power9/9 CTests96 Qt checks and fatal-warning lock3/3 CTests37 Qt checks, zero failures/skips; final MkDocs strict exit0 and511-document link/navigation check exit0.
- Initial manager configure rejected default /usr/local path against installed frozen capture paths; corrected to required /usr/libexec and successful configure/build followed. No product path guard bypass or source change.
- Independent reviewer ACCEPT source, policy matrix, signatures/image and regressions; durable verdicts are integrated. Extra reviewer fatal-warning cohort warning in unchanged sysfs empty-root fixture is documented as nonblocking, with required cohort green.
- Source main and laptop bare mirror fast-forwarded to the accepted integration; showcase working copy and all unrelated files preserved. Compiler/private-runtime leases released.
- Bounded caveat: physical prompt/focus/owner-password unlock has not been performed. Named service policy is installed for the next normal authentication attempt. Future desktop recipes must require the policy package per ADR-0349.
- Next action: use the installed repair; if an actual prompt problem recurs, obtain the specific native failure evidence without rebooting or changing authentication policy. Existing unrelated recovery work stays at its prior boundary.

All product work is preserved on qinda; no further compatible incident implementation is claimed.
