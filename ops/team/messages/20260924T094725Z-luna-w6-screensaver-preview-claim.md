# Claim — W6 screen saver preview repair

- Worker: Luna (`luna-w6-screensaver-preview`)
- Branch: `worker/claude-w6-screensaver-preview-20260923`
- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w6-screensaver-preview`
- Repairing the four blocking findings on top of reviewed candidate `b443f394483ee758f15fc6f0b2963aa3de2e563a` without rewriting history.
- KScreenLocker source and the desktop-controls architecture page confirm that `org.freedesktop.ScreenSaver.Inhibit` is honored for its automatic idle-lock timeout. Preview will fail closed when the inhibitor cannot be acquired.
- Planned verification: build Settings targets with `-- -j8 -l20`, run `ctest -R screensaver`, run the complete `ctest -L settings` suite, and run `./tools/validate-docs`.
