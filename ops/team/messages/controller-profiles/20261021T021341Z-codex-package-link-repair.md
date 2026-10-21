# Production shortcut client plugin link

The first r40 Portage build reached the final plugin link and failed because the production static shortcut client lacked PIC. The standalone fixture had substituted a PIC-enabled target and hid this requirement. The owning production target now sets PIC; the focused build consumes that actual target.

The corrected focused build, all 14 CTest groups (126 QtTest results, no failures/skips), strict MkDocs and 576-document link validation pass. The tracked r40 Portage patch preserves its immutable source archive. Only .prepared/.configured phase stamps were cleared; completed object outputs remained. The incremental Portage prepare/configure/compile/install/package finished successfully in 37.326 seconds (1.2 GiB memory peak). Signed image verification and laptop adoption follow. No model workers were used.
