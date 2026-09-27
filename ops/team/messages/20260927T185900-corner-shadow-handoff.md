# Corner shadow and caption handoff

Exact product candidates:419fe44e (shadow),1edc1fb6 (caption and all-theme contrast audit). Both preserved on qinda hub branch fix/corner-shadow-contour. Later commits contain board receipts only.

Changed paths: src/decorations/qindadecoration{,visuals}.cpp and visuals header; tests/decorations/{CMakeLists.txt,tst_decorationvisuals.cpp}; src/decoration_painter/src/decoration_painter.cpp; tests/decoration_painter/tst_decoration_painter.cpp; tests/design_tokens/{CMakeLists.txt,tst_builtin_contrast.cpp}; docs/wiki/architecture/hybrid-chrome.md. Implementation does not edit the KWin input patch or decoration property publication order.

Verification exits0: local isolated exact-source CMake build using installed unchanged theme/hybrid libraries; DecorationVisualsTest10/10; DecorationPainterTests15/15; BuiltInContrastTests5/5 including216 caption combinations and all18 shipped theme body/translucency checks; tools/validate-docs418 pages; git diff --check. Logs reside under ignored .cache/corner-shadow/{configure,build,visuals,paintertests,contrast}.log in worker worktree. Pinned KWin6.6.6 ShadowItem::buildQuads directly inspected: fixed horizontal corners and sampled vertical corners fit actual window bounds without redistribution. Native full-plugin build/CTest and physical/package application remain manager gates, not claimed here.

The shadow retains an exact-width shallow top strip, preserving tab edge and body shoulder; rectangle/member/maximized contracts are covered. Caption repair preserves existing colors above4.5:1, then prefers primary theme text, then stronger black/white. Translucent arbitrary backdrop contrast remains explicitly unqualified in docs.

Requested next action: independently review exact commits, integrate accepted candidates, run native plugin/CTest gates, deliver via overlay on both hosts. I read the shell queue and offer bounded repairs for any exact native regression; no compatible unrelated work claimed. Worker transitions idle after this handoff.
