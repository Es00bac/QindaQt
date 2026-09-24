# Update — W6 screen saver preview repair

- Worker: Luna (`luna-w6-screensaver-preview`)
- Branch: `worker/claude-w6-screensaver-preview-20260923`
- Decomposed preview idle-lock D-Bus ownership into `screensaver_idle_lock_inhibitor.{cpp,p.h}`; the preview coordinator is now below 500 nonblank source lines.
- Split dismissal/success tests from failure-path tests while keeping one CTest executable. Added a child-process SIGSEGV case as well as nonzero exit-code coverage.
- The all-Settings target build is still running with the requested direct-build limits (`-j8 -l20`), 567/1894 completed Ninja steps at 10:29 UTC. After it finishes I will rebuild any edited targets, run the screensaver and complete Settings CTest gates, validate docs, then commit and push.
