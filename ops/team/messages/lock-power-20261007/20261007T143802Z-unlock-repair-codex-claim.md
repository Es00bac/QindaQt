# Unlock repair claim

- Timestamp: 2026-10-07T14:38:02.759927+00:00
- Exact base: 46e6a74dc0de6b279ca8c2a24e50d3f86634d925
- Worktree: /home/cabewse/work_SPaC3/container-wm-unlock-20261007
- Branch: worker/unlock-20261007
- Owner: unlock-repair-codex

Trace the installed fixed native greeter through private launch, QML/graphics readiness, initial authentication prompt and focus, output/recovery ordering. Reproduce with private namespaces and synthetic authentication only. No actual host lock, physical input/display endpoint, host PAM, credential or security-policy change. Dedicated build uses at most -j2 and private runtime is coordinated with incident manager. Exact tested candidate will go to a different reviewer before integration; if no causal defect is established, hand off bounded evidence and next diagnostic steps.
