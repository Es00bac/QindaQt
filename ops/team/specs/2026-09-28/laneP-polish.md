# Lane P (Sonnet): close four known caveats from this round's lanes

- Worker name `claude-polish`, voice seat `claude-helper-three`, speech name "polish". Speak only at handoff.
- Worktree: `cd ~/work_SPaC3/container-wm && git fetch hub && git worktree add ~/work_SPaC3/container-wm.worktrees/polish -b fix/round-caveats hub/main`.
  Push to `hub`.
- Make one commit per item, each with a targeted test. Update the affected wiki or ADR text in the same commit.

1. **Container names survive a compositor restart.**
   - Today the user-set container name (default "Container", shown on the title tab) is lost when
     KWin restarts.
   - Wherever container membership and layout are persisted and restored, the name must round-trip
     too.
   - Test: a persistence round-trip that includes a custom name and the default.
2. **The Corner Bar title double-click rolls up.**
   - A stored `containerTitleDoubleClick=none` (a Settings default, not a user choice) currently
     overrides the Corner Bar theme's roll-up.
   - Find out how that value gets stored.
   - The theme's default action must apply unless the user explicitly picked an action in Settings.
     An explicit user "none" still wins.
   - Test both cases.
3. **The portal accent follows the active dark twin.**
   - With a dark twin theme active, `xdg-desktop-portal-qindaqt`'s Settings interface
     (`org.freedesktop.appearance` accent-color and color-scheme) must report the active twin's
     values, not the chosen base theme's.
   - Test with a twin whose accent differs.
4. **Touch pick-up lift feedback.**
   - A touch long-press that picks up a window or container title gives the same lift feedback as the
     mouse pick-up, using the existing animation and tokens.
   - Relevant code: `src/compositor/kwin/kwininteractionfilter_touch.cpp`,
     `hybridchrometouchpolicy.*` and `m_titlePickup`.
   - Test at the policy/state level. The visual needs a live check; list it.

## Rules

- Build only your targets on qinda with `-j8 -l24`. Run only the tests for the modules you changed.
  No full suite.
- Never touch the live session.
- Keep token use low: grep to find, read only the relevant ranges.
- Report commits, files, tests with counts and exit status, and the live checks.
