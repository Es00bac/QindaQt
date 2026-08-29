# Claim: Display Color C0 pure model boundary (QQ-005.07)

- **Date:** 2026-08-28T12:11:00-06:00
- **Author:** Solene Ward (Google Antigravity Vertex ADC, `gemini-3.7-flash-high`, reasoning: high)
- **Role:** Display Color C0 model implementer
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/display-color-c0-gemini-solene`
- **Branch:** `worker/display-color-c0-gemini-solene`
- **Base Commit:** `146fc48358c2659436dec4fc6b6062d23c5ee746`
- **Status:** Claimed / Working

## Summary

I have claimed the Display Color C0 pure model boundary outcome (QQ-005.07) from clean base `146fc48358c2659436dec4fc6b6062d23c5ee746`.

## Scope and Boundaries

- **Owned paths:**
  - `src/services/display_color_model/**` (new module)
  - `tests/services/display_color_model/**` (matching focused tests and test support)
  - Smallest additive CMake registration in `src/CMakeLists.txt` and `tests/CMakeLists.txt`
  - Owning documentation: `docs/wiki/architecture/display-color-model.md`, `docs/wiki/adr/0030-display-color-c0-model-boundary.md`, updates to navigation / module boundaries / testing harness / docs metadata
  - Worker record `ops/team/workers/solene-ward.md` and message threads under `ops/team/messages/platform-display-color/`
- **Prohibited paths / actions:**
  - Do not edit Display1 existing modules (`display_protocol`, `display_identity`, `display_topology`, `display_transaction`, `display_service`), Settings/Appearance, features/TASK_LIST/HANDOFF, or integration branches.
  - No profile parsing beyond bounded header/metadata validation.
  - No filesystem enumeration, import writes, compositor mutation, GUI, or hardware access.
  - Never touch host displays or host color configuration.

## Key Contracts and Outcomes to Implement

1. **Injected ICC profile descriptors & header validation:** Bounded validation of ICC header (magic `'acsp'`, version, profile/device class, color space `'RGB '`, profile connection space `'XYZ '`/`'Lab '`, size limits, MD5/digest validation).
2. **Validated import metadata:** Safe filename/display-name validation, origin classification (BuiltIn, System, UserImported, EDIDDerived), size limits (e.g. max 4 MiB), checksum integrity.
3. **Stable per-output assignment intent:** Mapping persistent display stable IDs to profile assignments, intent modes (Perceptual, RelativeColorimetric, Saturation, AbsoluteColorimetric), calibration state, and fallback to default sRGB.
4. **HDR / WCG capabilities & policy:** Output capabilities (sRGB only, WCG/DCI-P3/BT.2020, HDR10/PQ, HLG), peak luminance, max frame-average luminance, min luminance, transfer function support; output color policy values (SDR sRGB, SDR WCG, HDR enabled, auto HDR/color management mode).
5. **Deterministic catalog ordering & resolution:** Stable sorting of available profiles by origin, display name, and unique ID; resolution of assignment by stable ID; last-known-good (LKG) fallback on invalid/missing profiles.
6. **Identity & lineage validation:** Validating display stable IDs (matching Display1 format / non-empty), lineage epochs/revisions, rejection of stale or out-of-order publications.
7. **Atomic publication & degraded/unsupported truth:** Atomic model snapshot generation, truthful reporting of unsupported/degraded capabilities (e.g. HDR requested on SDR hardware, missing profile descriptor, corrupted ICC header).
