# Post-reboot service repair claim

Outcome: ordinary Bluetooth controls can activate their selected-radio helper;
the owner also receives an evidence-based audit of the current laptop boot.
Base `3a0980fc8`, isolated branch/worktree `fix/post-reboot-radio-20261017` /
`.cache/post-reboot-radio-20261017`. Ownership and executable acceptance are in
`ops/team/workers/post-reboot-services-codex-20261017.md`.

Observed: helper sandbox denies the ordinary session-bus socket; the adapter
was software-blocked. Selected unblock restored BlueZ Powered=true. Voice1 was
idle/enabled but reported Media Previous and neither Gabbee binding existed in
Shortcuts1. Saved Media Next and gracefully restarted only the idle provider;
both bindings now exist and are active. Physical-key confirmation is pending.

Other evidence: 65 short s2idle cycles and six OOM kills during this boot. Main
Qinda service snapshots respond. The initial virtual-camera module-load error
does not establish a current failure: the packaged module is present and loads.
