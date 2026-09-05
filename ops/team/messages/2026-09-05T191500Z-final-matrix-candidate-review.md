# Final matrix candidate review: `8be6f9c242c0034cadb338feed54a598a1bca6fe`

- Range reviewed: `7e28c807..8be6f9c`.
- Result: ACCEPT.
- Focused evidence: `PYTHONDONTWRITEBYTECODE=1 PYTHONPATH=tests/session python3 -m unittest test_desktop_session_interactive_unit test_desktop_session_matrix_unit test_notification_live_unit` passed 25/25, exit 0. `git diff --check` passed.
- The correction removes the unconditional task-list requirement. Every declared task-list applet remains strictly validated for readiness, canonical entry point, panel identity, and duplicate-per-panel rejection; a launcher remains required. QindaQt retains the exact smart-shelf launcher/task-list composition check. GNOME fixtures correctly use overview-trigger plus launcher without inventing a task-list.
- `validate_interactive_evidence()` derives the smart-shelf requirement from the already validated matrix profile metadata. Documentation records the profile-aware qualification rule, and the package-contract test remains registered.
