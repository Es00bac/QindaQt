# Corner preview handoff

Exact product candidate: `4f21effb`, pushed to hub branch `fix/corner-preview-silhouette`.

The shared unchanged shadow texture now renders in Settings, client backing preserves the title cutout, and final decoration painting excludes the floating-point client rectangle so rounded title material stops at the seam. Final correction owns `appearance_preview_window.cpp`, focused silhouette tests, and Appearance documentation; earlier extraction paths remain as assigned.

Evidence: local focused preview executable exits 0, 6 passed / 0 failed, including Corner Bar square, rounded, HiDPI, ordinary title opacity, and below-seam client-color assertions. Earlier local decoration visuals passed 10/10 and prior/new texture comparison passed all 64 cases byte-for-byte. Root owns final native CTest rerun and both-host r7 delivery. Independent reviewer has the exact candidate; acceptance is pending that final check.

Requested action: review `4f21effb`, run native `qindaqt.appearance-window-silhouette`, then package and install on both systems. No further scope is proposed.
