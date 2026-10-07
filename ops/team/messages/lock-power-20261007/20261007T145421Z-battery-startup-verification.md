# Battery startup verification

- Time: 2026-10-07T14:54:21Z
- Worker: Battery Startup Codex
- Exact base: `46e6a74dc0de6b279ca8c2a24e50d3f86634d925`
- Build root: `.cache/battery-startup-build` in the assigned qinda worktree; dedicated Debug, strict warnings, optional shell/plugin targets disabled, /usr prefix with libexec configure destinations, existing ccache, maximum -j2. Nothing installed.
- Build command: `cmake --build .cache/battery-startup-build --parallel 2 --target qindaqt_power_upower_startup_tests qindaqt_power_upower_adapter_tests qindaqt_power_production_activation_tests qindaqt_power_internal_backlight_apply_tests qindaqt_power_upstream_composition_tests qindaqt_power_service_publication_tests qindaqt_power_service_operation_tests qindaqt_power_service_residency_tests` — exit 0.
- Test command: `ctest --test-dir .cache/battery-startup-build --parallel 1 -R '^qindaqt.power-service-(upower-startup|upower-adapter|production-activation|internal-backlight-apply|upstream-composition|boundary|publication|operations|residency)$' --output-on-failure` — exit 0, 9/9 CTests, 96 Qt checks, 0 failures/skips; 10.86 seconds.
- Startup evidence: Seven behavioral rows/9 Qt checks prove dormant production composition (28 percent charging plus actual private backlight inventory), failed activation, 3-second timeout followed by later owner recovery, stop, restart, destruction, and competing owner arrival. All activation uses a descriptor on an explicit private bus with no host activation directories.
- Regression sensitivity: The same dormant-production row rebuilt with the exact base adapter source exits 1, zero supplies versus expected one. Restored fixed source rebuilt before the final cohort. Logs retained only in ignored `.cache/battery-startup-evidence`.
- Initial bounded corrections: The private starter address includes D-Bus's generated GUID; helper validation now compares its pinned endpoint. The base production descriptor test rejected generated trailing whitespace when optional policy arguments are empty; its focused assertion now trims that whitespace and retains the required production mode.
- Documentation: `mkdocs build --strict --site-dir .cache/battery-startup-docs` — exit 0. The prescribed CTest docs/links regex has no registered test in this configuration; direct `tools/validate-docs` — exit 0, 510 Markdown documents and navigation validated.
- Hygiene: `git diff --check` — exit 0; direct process scan shows no remaining startup test/helper/private-daemon rows. Adapter production source is 422 nonblank lines.
- Scope caveat: Strict Debug private fixture evidence only; installed package and physical fresh-login qualification are manager-owned subsequent gates. No laptop builds or host daemon actions.
- Next action: Commit this verified candidate and request independent exact-commit review.
