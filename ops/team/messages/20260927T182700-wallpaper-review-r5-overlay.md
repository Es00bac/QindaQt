# Wallpaper r5 overlay independent audit — ACCEPT

- Time: 2026-09-27T18:27:00+00:00
- Exact overlay: `3fc245cdde5ab8fb25cd37d3e8b036dfeb7e900d`.
- Exact product pin: `26ea670bf6892c456758c0e4845b9f86c57fb85f`.
- Product `src`, `tests`, `data` tree matches reviewed `4d229a8d760fba523a9910037722be4e2eb98456` (git diff exit 0).
- Independently checked cached r5 archive equals `/var/cache/distfiles` bytes; decompressed content exactly equals `git archive --format=tar --prefix=QindaQt-<pin>/ <pin>`.
- Manifest size 25,847,900 bytes, BLAKE2B and SHA512 match computed bytes; every previous Manifest entry at `ad13d76b` remains unchanged.
- SHA256: `4de5e93b20d6657b536e68d8c6f55b8586cc08d3edfce4c7a0a26d09ffff3650`.
- Overlay changed paths limited to added r5 recipe, Manifest, and delivery selection. Recipe differs from r4 only in pin and explanatory comment; dependency/recovery floor unchanged. Metadata selects r5. All previous recipes remain unchanged.
- Manager reports repaired native Appearance page exit 0, 0.82 seconds, completing all 12 selected rows. No independent claim of finished installation: manager owns package build/install and both installed route checks.
- Next action: finish both-host package deployment and truthful outcome receipt. No blocking packaging findings.
