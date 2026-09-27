# Native Corner Bar cutout verified

The finalized fixture is compositor/tests/transparent-decoration-input.patch.
It applies to upstream9bf2235f after the unchanged runtime patch. Compilation
uses only public integral geometry and workspace move/resize ownership.
Pointer binding precedes mapped-window waits; explicit private Wayland sync
flushes motion/button transport before client event assertions.

Native build: `cmake --build` retained KWin6.6.6-r1 tree, target
`testDecorationInput`, exit0. Positive wrapper run: exit0,3/3 QtTest rows,
116ms. Actual lower-client pointer entry and two button events, retained upper
frame, retained title hit and property removal/type fallback all pass.
Negative wrapper run against retained original exact-ABI6.6.6 library: exit1,
2pass/1expectedfail,118ms, precisely `!above->hitTest(cutoutPoint)`.
No physical session input or compositor restart was used.

Evidence logs on qinda under ~/.cache/qindaqt-corner:
input-final-build.log, input-final-positive.log, input-final-negative.log.
Runtime patch did not change. The regression requires packaging it, which
manager separately owns. Request exact final fixture review, then integrate.
