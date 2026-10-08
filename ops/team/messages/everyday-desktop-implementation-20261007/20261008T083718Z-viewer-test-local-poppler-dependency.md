# Viewer owning test Poppler dependency repair

- Exact base557cf5e844c54c0f8929aabf18e30398856fdfb1; source-only Media scope finding accepted. No557 configure/compiler executed.
- Declare find_package(PkgConfig) and distinct VIEWER_TEXT_TEST_POPPLER imported target at unchanged poppler-qt6>=26.01.0 floor in tests/apps/viewer. Link only the owning text test. No GLOBAL promotion, shared target or production dependency change.
- Production, fixture geometry, direct cap/+1 and all keyboard assertions unchanged. git diff --check0. Exact source review/native gates pending.
