# First-party live appearance candidate

- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/app-appearance`
- Exact base: `2981a2865f1302612e6cc1e14cc566d6e4b1d991`
- Exact candidate: `4f166d31b6c1c15ef2b9646c30b373e05bc4d460`
- Public module: `QindaQt::AppAppearance`, pure resolver plus borrowed-SettingsClient retained controller and TokenFacade publication seam.
- Policy: compatible selected theme wins; Light/Dark/System resolve incompatible choices to qinda-light/qinda-dark; System unknown deterministically dark; high contrast remains explicit; CLI theme locks process. Invalid/missing snapshots retain last valid theme.
- Consumers: shell and Settings preview share resolver; theme-card drafts matching scheme; Editor updates application/window/document views; Terminal updates application/window/current session adapter; File Manager republishes its engine TokenFacade.
- Docs: ADR-0079, module boundary, three app pages, nav/index.
- Gates: git diff --check PASS; tools/check-source-shape PASS; mkdocs strict PASS; validate-docs PASS 173.
- Build caveat: intentionally no fresh build. Candidate needs Welcome owner's additive `src/CMakeLists.txt add_subdirectory(app_appearance)` before compilation; exact independent review requested first.
