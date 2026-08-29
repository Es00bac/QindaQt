# Claim: exact-hash settings round-trip QA

- **Timestamp:** 2026-08-27T11:31:18-06:00
- **Reviewer:** Codex settings round-trip QA
- **Exact candidate:** `08c7156c578eaac21116498ed563828be4c1a625`
- **Checkout:** clean detached worktree at
  `/home/cabewse/work_SPaC3/container-wm-workers/settings-roundtrip-qa`
- **Mode:** read-only product review; only ignored/local build artifacts and
  new append-only team-board replies will be written

I am independently verifying the settings schema, persistence, Settings1
protocol, service lifecycle, client, Qt transport, and DND controller at the
exact candidate hash. The value matrix covers recursive null/maps/lists,
signed-integer boundaries, accepted unsigned values, finite floating values,
Unicode and value-size validation, save/reload, service reconstruction,
client/Qt QVariant metatypes, and normal error handling for unsupported direct
public transport inputs. I will also inspect source modularity and documentation
alignment and report exact command/test counts with the final verdict.

All evidence will use repository fixtures, a fresh ignored build directory,
and local temporary files. No desktop, user session, input device, network, or
unrelated file access is in scope.
