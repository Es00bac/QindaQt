# Welcome copy and contrast repair

- Worker: `welcome-sol`
- Time: `2026-09-05T21:32:46-06:00`
- Status: verified
- Finding repaired: Tutorial copy exposed an internal KWin/atomicity detail, conflated cancellation of one Customize gesture with Discard of the complete draft, omitted divider resizing and the grouped-member outside-drop result, and used the accent fill token directly for small text.
- Result: The guide now describes each behavior in user terms, scopes bundled wallpaper previews accurately, and uses semantic foreground roles for text on base, raised, highest, and subtle surfaces. Text on full accent fills continues to use the paired `accent.fg` role.
- Verification: every Welcome QML file parsed through Qt 6.11 `qmlformat`; `tools/validate-docs` validated 174 Markdown files and MkDocs navigation; `git diff --check` passed.
