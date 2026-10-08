# User stop: exact Android preparation boundary

User stop takes precedence. No further preparation, access probe, quickpkg, pretend, fetch, build, merge, guest or dependent metadata action will run.

The already-issued root-authorized preparation call completed before the stop arrived. Contrary to the manager stop message's initial assumption, /var/tmp/qindaqt-android-prereq-20261008T181609Z DOES exist. It contains only the fresh task workspace/nonsecret config snapshot and root-only backups/pin authority. The completed call reported103 configuration source pins,48 recipe/eclass/Manifest pins, four missing source archives and NSS account names/IDs absent. No source archive was fetched/copied; no package phase ran.

The private backup contains original account/PAM files and installed libcap VDB for preservation, with no secret contents printed or committed. Global account/config/library/world/profile data was not modified. No quickpkg, pretend, build, merge, runtime initialization, mount or VM started. All owned tool calls settled; no asynchronous process or resource lease remains.

Preparation is UNQUALIFIED: effective owner/mode traversal and actual Portage-user access probes have not run. In particular the preparation used umask077 and initial mkdir modes; before any future access, root must verify actual ancestor/home modes against accepted0710/0750 rather than assuming requested mkdir modes survived umask. No post-stop chmod or other correction was attempted. Preserve the private root as unfinished evidence; do not delete or silently reuse it as admitted input.

Read-only API diagnostic is also retained: getFetchMap(myrepo=...) raised TypeError; installed signature was inspected and the corrected mytree metadata-only read succeeded. All four archive names are absent in /var/cache/distfiles: libnftnl1.3.2,libcap2.78,libglibutil1.0.82,gbinder1.1.52. This was not a fetch/build failure or authorization to fetch.

Own ignored summary: .cache/android-prerequisite-transaction/stop-boundary.json. Actual protected pin journal stays /var/tmp/qindaqt-android-prereq-20261008T181609Z/private/preparation.json; no new reads after stop. Accepted8601 proposal and02ce review remain prospective. Resume only after user returns and root routes unfinished preparation, then the separately bounded first two phases; merge remains ungranted.
