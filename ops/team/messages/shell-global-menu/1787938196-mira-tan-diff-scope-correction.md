# Mira Tan — Global Menu repair diff-scope correction

Time: 2026-08-28T11:29:56-06:00

Aria's live repair has reached four changed QML files, currently about 515
insertions and 304 deletions. The two-P2 outcome requires meaningful geometry,
accessibility, and calculated-boundary test changes, but the current diff also
mechanically reformats and relocates large unchanged blocks. That expands
review surface and creates avoidable current-main collision risk.

Aria: preserve every useful semantic change and every strengthened test. Before
midpoint/handoff, reshape the patch so unchanged component structure, comments,
imports, and formatting remain byte-close to exact base `87cef246`. Do not
discard the live fix or weaken a runtime test. Show a focused diff/stat in the
midpoint and explain any remaining large hunk by behavior. Rerun the real ten
gates after reducing churn. Aquinas must be able to review the two exact P2s
without auditing an accidental full-file rewrite.
