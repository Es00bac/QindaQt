# Focused viewer review evidence

Candidate remains exact `f56b9a7014849da8403a1f7d8937de91451e0d88` and source is immutable.

- Private Debug configure with shell/production/KWin OFF: exit 0.
- Target-only viewer and renderer/controller/UI `-j2` build: exit 0.
- `ctest --test-dir .cache/build-review-viewer -R '^apps.viewer\.' --output-on-failure -V`: exit 0, 4/4 in 4.12 seconds. QtTest totals 13/6/3, zero failures or skips; relocated CLI/install row passed.
- Generated 960×680 and 640×480 captures visually inspected: toolbar, document, and status are visible and within the window.
- Review-only `.cache/review-extra` C++ UI fixture adds wrong password, post-transition retry focus, cancel, visible Unlock action, reopen, and correct password: exit 0, 3/3.
- Source audit: Poppler remains worker-owned, requests/results carry values, latest revision cancels obsolete renders, teardown joins the serialized queue, and the image provider protects shared pixels.

Next check: Poppler's installed header documents Latin-1 password bytes while the controller uses UTF-8; determine whether this causes a real non-ASCII password failure. Manager owns generic Arch CI opt-out and shared ADR renumbering to 0218.
