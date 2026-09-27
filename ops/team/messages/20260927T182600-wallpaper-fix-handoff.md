# Wallpaper candidate handoff

Exact product/test candidate: `4d229a8d` on pushed `fix/user-wallpaper-gallery`.

Changed paths: Settings Appearance catalog/model/QML and CMake; focused catalog/page tests and stub; Appearance wiki, ADR0279 and navigation; own worker records.

Evidence: standalone Qt6.11.1 C++/moc build exit0; catalog5/5 exit0; documentation418documents exit0; whitespace exit0. Manager reports11/12native rows passing including model, installed routes and shell controller; imported card reselects correctly. Sole page failure was a test fixture attempting a nonexistent QtFileDialog selection. Candidate4d229a8d fixes that fixture using an existing nonimage, awaiting native rerun and independent exact review.

Requested next action: rerun page gate, integrate accepted candidate, publish normal package/overlay and install on both hosts. Bounded behavior: imported files remain when draft reverted; folder preference immediate/local; folder contents are not synchronized between machines. Existing Settings process needs reopening after package update. Actual wallpaper Apply remains shared Settings1 draft authority.

Read the First-party queue after handoff. Concrete help offer: available to repair any exact native or installed UI regression for this candidate; no unrelated stale queue assignment claimed.
