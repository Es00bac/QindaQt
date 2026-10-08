# R18 resolver failure and source-only repair — 2026-10-08T07:09:48+00:00

- Exact initial desktop source recipe: 14e440673b488ae8d77698bf56abb35345340766.
- Exact initial overlay recipe: af4e8ed791a3dd6d1c4ce617c87d048eb58b0c60.
- Independent source review: aca238228906b83a1d553eb7bf4460c0837850d7.
- Runtime freeze remains 5858bccdf82808a74a6bab358d8e6f08c7ac256a.
- Archive remains 39,433,447 bytes, SHA256
  620ad2fcfb7c006b55fa42b0f5e54a856ab888f29fa199b0c7072e8a0962f323.

The actual private signed Portage build exited one during dependency resolution,
before configure, compilation or signing, because the dev-libs/dbus atom I
specified does not exist in Gentoo. aux_get metadata acceptance did not qualify
the resolver. Platform preserves original log/status/argv and exact failure
receipt 4fe2a58ac; reported build log SHA256
a275dc77639824607a67ced3040584738b1265119b17d66065d40be0273e8114.

Direct installed-package/provider evidence identifies sys-apps/dbus-1.16.2 as
the owner of dbus.h, libdbus-1.pc and libdbus-1.so. This correction retains the
accepted libdbus architecture and changes its Gentoo dependency category.
Platform owns a source-only minimal paired recipe fix: explicit sys-apps/dbus
DEPEND and existing RDEPEND, unchanged archive/runtime/body/policies.
The next gate is actual private emerge --pretend --buildpkgonly resolution,
then exact successor review by the same Astra reviewer. A new real build lease
requires that acceptance; no automatic build, install, helper activation or
host-service action is authorized by this packet. Both native lanes are free.

R17 remains installed. The owner confirms Audio device discovery and reports
wasted space in both Settings and the popup. Accepted compact source has
37 integrated CTests/273 Qt checks plus owner-scale pixel acceptance, but is
not installed. New Clipboard, Power and Bluetooth repairs are source/native
qualified; their installed and hardware gates remain open.

The full remaining scope is ED01–24, Android/Windows, actual AI provider
feeds and the complete normative mixer. Best manager planning estimate is
50 working days, plausible 30–80; immediate core/UI delivery is 1–3 days.
This credits integrated progress without claiming whole user journeys from
candidates, tests or release numbers. Neither foreign runtime has a complete
real-app journey. Original audit/plan supplied no numeric ETA.

Manager documentation gates: tools/validate-docs exits zero (533 documents);
mkdocs build --strict exits zero (8.23 seconds); git diff --check exits zero.
Runtime source/tests/compositor/CMake remain unchanged by this checkpoint.
