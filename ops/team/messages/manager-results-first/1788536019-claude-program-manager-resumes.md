# Program Manager — Claude resumes integration of `main` (2026-09-04T09:33:39-06:00)

The user handed program management back from Codex to Claude with fresh Codex, Kimi, and GLM usage windows and the order
"work together and finish this". Standing orders are unchanged: make the desktop work on this machine, no new features, one
funded review round per candidate, lowest token spend, highest quality, truthful reporting.

## Baseline

- `main` = `e742d632`, clean; Release-install ready on the system KWin 6.6.6 (broad safe Release 592/592). The real
  `/usr/local` install and SDDM login smoke still require the user's sudo.

## Funded now (2026-09-04 09:45 MDT)

| Lane | Outcome | Worker | Reviewer (recheck) |
| --- | --- | --- | --- |
| `task-list-hosting` | QQ-004.10 T3: host the registered Task List applet in the production panels over the shell's exact-owner CompositorShell1 client; dispatcher count eight → nine | Vera Rubin (OpenAI Codex `gpt-5.6-sol`, high) on `worker/task-list-hosting` from `e742d632` | Henrietta Leavitt (Moonshot Kimi `k3`) |
| `tray-applet-repair` | QQ-004.11 Tray S2: repair the single P1 (degraded presentation not projected; no acknowledgement path) of `b10692c` | Sijue Wu (Moonshot Kimi `k3`) on `worker/tray-applet` | Rózsa Péter (OpenAI Codex) |
| `color-settings-route-repair2` | QQ-006.05 Color route: repair the single P1 (eager Color client warns under an unreachable session bus, aborting three fatal-warning host rows) of `252b2fd` | Maryam Mirzakhani (Moonshot Kimi `k3`) on `worker/color-settings-route` | Maryna Viazovska (OpenAI Codex) |
| `bluetooth-pairing-repair2` | QQ-005.05 pairing: repair the single P2 (ambiguous window-context Escape shortcuts in the real Settings host) of `7025a1c` | Rosalind Franklin (Z.AI GLM `glm-5.3`) on `worker/bluetooth-pairing` | Kathrin Bringmann (OpenAI Codex) |

Each candidate was one finding from acceptance; this is the one funded repair round for each. A tray hosting lane (GLM flash,
mirroring the task-list hosting pattern) opens only if Tray S2 is accepted. Lane briefs: `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/<lane>/`.
The shared brief contracts now name the system-KWin initial cache.
