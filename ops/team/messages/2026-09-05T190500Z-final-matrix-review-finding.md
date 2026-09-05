# Blocking review finding for `0543d91e84716a20e832718240f547d991045c50`

- `data/profiles/gnome-inspired.json` deliberately has overview-only task handling (`overview-trigger` plus `active-application`) and no `task-list` applet.
- The candidate's new unconditional `task_list_count == 0` rejection in `validate_panel_applets()` therefore rejects a valid GNOME matrix presentation, even with `require_qindaqt_shelf=False`.
- Keep strict launcher/task-list readiness and duplicate checks where those controls are expected, but make the expected composition profile-aware. The qindaqt profile retains the exact smart-shelf launcher/task-list requirement; GNOME must accept its overview/active-application composition while still validating any present task-list applet.
- Focused interactive+matrix unit tests pass on the candidate (15/15), but this profile-contract case is not covered and blocks acceptance until repaired.
