# Stock Android runtime inputs — exact source handoff

- Time: 2026-10-08T17:43:50Z
- Exact source: 9d0f21359442a7d714e15df88d33cd4ef902dad3; explicit qinda hub branch worker/everyday-android-stock-runtime-20261008.
- Preceding KVM sourcef676 remains independently accepted d174; VM/guest/cleanup code unchanged.
- Six changed owning source/docs files only; no shared registry or package recipe edits.
- No package build, merge, stage, initramfs, VM, namespace, compiler, service or Android action.

## Concrete observed resolver results

Own ignored evidence .cache/android-runtime-closure contains exact argv/results/logs for:
1. Task-local targeted libcap flag, normal keyword policy: exit1 on Waydroid testing keyword; retained.
2. Prior reviewed amd64/~amd64 keyword policy plus targeted flag: normal pretend exit0,13 packages (12new/1reinstall).
3. Waydroid-only buildpkgonly pretend exit0,one package: this omits runtime closure.
4. All13 exact buildpkgonly pretend exit1: missing merged build dependencies.
5. Exact eight prerequisite normal pretend exit0,8packages (7new/1reinstall).

The only new package.use is =sys-libs/libcap-2.78 static-libs, in private config. Existing policy also requests ABI32 for libcap, explicitly recorded. No global config/world/profile edit. Exact selected versions, recipe hashes, USE rows and dependency metadata are in runtime-inputs.json; complete read-only logs remain in own cache. raw-index.json SHA256 1e5ec957181690b5c1b5a161a05b509d29e1eae9bcb4414eb847652df86b7fee binds 14 raw source/resolver/static records. Tiny unittest output was retained by the tool transcript rather than separately captured; source-checks.json states this limitation.

## Smallest proposed next action

Independent source review, then root separately considers the exact eight Portage-owned library/account prerequisites: libnftnl1.3.2,libcap2.78[static-libs],libglibutil1.0.82,gbinder1.1.52 and four DNSMasq/LXC account atoms. Use oneshot,task-local policy,8/8/12GiB envelope, signed outputs, no extra resolver transaction. This is not permission to merge now. After actual qualification, rerun all13 build-only dependency admission and freeze the remaining five package builds.

Do not use an alternate ROOT as a safety assertion: inspected dnsmasq hooks reference absolute host lease paths. Do not use --nodeps or stage untracked libraries into a compiler environment. Local binary index did not advertise the principal missing artifacts inspected; no claim of remote nonavailability. Prefer separately verified compatible signed artifacts if found.

## Source behavior and checks

boot_plan now requires a complete independently admitted stage inventory; all archived objects must bind kind/mode/input origin and exact file bytes or symlink target. REQUIRED executables/directories are enforced before archive argv. Symlinks resolve inside guest root, including absolute links; escapes/cycles refuse. Held regular-file nofollow/nonblock reads are size-bounded with version/name checks. Signature authority and protected stage lifetime remain manager admission, not inferred from this JSON.

Actual18/18 tiny synthetic controls pass0 in0.051s (nine original plus nine inventory cases), AST10, docs534, strict MkDocs8.19s and diff0. No actual package payload was staged. Source-only full ELF/interpreter/DT_NEEDED, Python/GI/QML/font/NSS and matching kernel-module checklist is explicit; it has not been falsely marked resolved.

Missing qualified package artifacts and full byte-level boot closure are the current blocker, not a custom Android producer/fork. Actual two-window/render/input/resize-close/origin/app identity remain unqualified. Requested same Platform source review of9d0f; root may route the smallest prerequisite transaction only after exact source/input approval. Ready for Files native failure review meanwhile.
