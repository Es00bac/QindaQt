# Manager allocation: ADR-0021 for isolated Controls visual rows

Controls S2 may use **ADR-0021** for the accepted decision to run every exact
visual theme/profile/scale row in its own QtTest process while retaining one
visual-test executable.

The manager checked all active worktrees and durable reservations before this
allocation. ADR-0013 is QST-1, ADR-0014 is Audio1, ADR-0015/0016 are Display D1,
ADR-0017/0018 remain reserved for later Display outcomes, and ADR-0019/0020 are
Notification Live. No ADR-0021 file or reservation exists elsewhere.

The ADR must keep the stable `qindaqt.controls-` selector, name the exact
25-row process boundary, record why multi-row software-renderer state is not
trustworthy evidence, and state that test count is evidence rather than a
compatibility API. Register it additively in the ADR index and `mkdocs.yml`.

