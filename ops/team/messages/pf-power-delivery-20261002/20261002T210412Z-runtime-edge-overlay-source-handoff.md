# Native runtime-edge overlay source handoff

- Recorded: 2026-10-02T21:04:12Z
- Exact candidate: 851e2c0f6ae434128db517946b5f391101d3b674
- Base: 502a04237ed9e36832f1f578a2f3716d5959c367
- Branch/WT: worker/pf-native-runtime-edges-20261002 / qinda:~/work_SPaC3/QindaGentoo.worktrees/pf-native-runtime-edges-20261002
- Pushed to qinda bare hub; working tree source clean.
- Requested next gate: different-worker exact source review; then manager-granted matching-version build/real QML and KDEConnect daemon/app smoke plus native Polkit agent registration. No source-only completion claim.

## Changes and actual bounded evidence

KAuth6.27-r1 retains +policykit, actual effective policykit and PolkitQt6 backend/security; only KDE session agent PDEPEND omitted because the native session supplies it. Kirigami1.12.1-r1 retains all modules/code and other dependencies; only unused libplasma runtime atom omitted. KDEConnect26.04.3-r4 keeps default-on +plasmoid for stock compatibility; native profile disables only its package-only applet and matching libplasma atom, with daemon/app/indicator/SMS/CLI retained. Original patches/license/metadata credits retained. Four additive native profile USE lines disable Sloom panel integration, xdg-utils Plasma integration, LXQt monitor module and KDEConnect applet; no app removal or world mutation.

Three exact official unchanged source archives measured and bound to Manifest. Kirigami/KDEConnect DIST rows byte-match original Gentoo; KaAuth6.27 current Gentoo no longer carries that old row, so its actual upstream downloaded bytes have newly recorded hashes (not claimed as preserved historical checksum). Exact source/patch/recipe SHA256 and all18 paths are in adjacent JSON/provenance. The actual release Kirigami archive independently contains223 inspected source files and8 nondependency Plasma token hits.

Six focused checks actual PASS0/0.227s/0fail0skip: native policykit and complete addons deps, stock/native applet metadata, actual Gentoo cmake helper's enabled/default byte-identical CMake and disabled only-package-subdir CMake. The helper test isolates the new decision; unchanged ecm preparation/old patches are deliberately not executed or falsely claimed. Bash syntax3/3 exit0; actual private Portage metadata3/3 and three integration effective-OFF flag assertions exit0. Existing final Desktop/fork/Gabbee/delivery/profile parent+package roots/KIO objects exactly equal502.

Pkgcheck exit0 retains preexisting empty IUSE/stable keyword suggestions, and important KAuth6.27 nonexistent cold-source dependencies: matching KF coreaddons/windowSystem6.27 are installed but removed from current Gentoo recipes. New metadata.xml copies fix initial missing upstream descriptor finding; no subsequent scan result invented. This exact matching-version candidate is not a cold/recent-version Frameworks build. Full raw diff-check exit2 only on byte-exact upstream patch-format whitespace; authored-only diff-check exit0. Preserving upstream patch bytes is intentional.

No compiler, package install/fetch into host distdir, world/profile/host config modification, unmerge, service restart, physical action or game path occurred. Official archives and private Portage metadata/source checks live only in task-owned cache. Parent and packaging reviewer notified with fullSHA; no active own compiler/runtime.

## Material remaining decisions

The retained userapps may still have OR-provider choices (Claude) or unrelated world/set roots; the earlier0c1c98234 read-only two-host plan remains exact timestamped evidence. Existing393 two-root closure does not prove full installed world closure. KAuth future/current source6.29 needs separately matching recipe review rather than loosening6.27 exact dependencies. Later Kirigami/KDEConnect versions need new matching adaptation; no arbitrary upgrade was introduced here.

A positive source review must be followed by actual builds, addon QML smoke and KDEConnect normal app/daemon smoke with no Plasma applet, and actual native Polkit agent registration before old agent retirement. Root owns installation/removal/fresh login; no forced unmerge or app deletion proposed. Available for the smallest reviewed continuation or bounded matching-version source repair.
