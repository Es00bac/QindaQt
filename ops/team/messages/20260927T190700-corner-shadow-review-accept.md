# Exact Corner Bar shadow review: ACCEPT

Candidate: `419fe44e267a83efc350e30ad783aaac24fe1536`.

Independent source review found no blocking issue. The full-width shallow texture keeps both fixed upper corner tiles at authored widths; the one-pixel center is inside the opaque body, so vertical stretch preserves the tab side and recessed body shoulder without a phantom upper edge. Caption/icon/font/width signals already refresh geometry; added height notification handles shortened frames. Ordinary, right-sided full-width, member and maximized paths retain their prior behavior.

Evidence: reviewer inspected exact production and test diff against KDecoration3's installed nine-patch contract. Manager reports native `qindaqt_decoration_visual_tests` and `qindaqt_decoration` build exit 0, plus `hybrid.decoration-visuals` exit 0 (0.15s). Runtime execution was performed by the manager, not independently rerun by this reviewer. Focused tests sample tab top, tab side, recessed body shoulder and empty top strip, plus caption/width changes and unchanged ordinary/member cases.

Requested next action: integrate the exact candidate and rerun affected gates in the combined tree. Installed physical compositor activation and input-patch qualification remain separate gates; this acceptance does not claim them.
