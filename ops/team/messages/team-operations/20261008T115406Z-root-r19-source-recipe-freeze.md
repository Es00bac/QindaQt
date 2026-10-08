# Prepare immutable R19 source/recipe

- Timestamp: 2026-10-08T11:54:06+00:00
- Runtime freeze: e884c310d009b5c45b1c82aec81c1a554febbd1d
- Previous immutable release: r18 at5858bccdf82808a74a6bab358d8e6f08c7ac256a
- Archive: qindaqt-desktop-0.1.0_pre20261002-r19.tar.gz
- Bytes:39521461
- SHA256:a7158408158d8fd756c2481aae521add97a86c778e2a1ff4e147ca9ba08e42dc
- New recipe SHA256:3309ed961b3456ae2c7a408d534535fc482837ec881192bf328d108db658c930
- Overlay base:a829b04fd9180a427d4485954103667943f7632e
- Overlay author: /home/cabewse/work_SPaC3/QindaGentoo.worktrees/everyday-r19-20261008

Two separate git archive/gzip-n cuts match; all9803 tracked regular blobs (32 executable) and Git-default tar modes match the immutable source. New desktop and overlay recipe bytes/DIST row match. Both historical Manifest prefixes are preserved. The recipe differs from r18 only by immutable source pin and first two comment lines; no dependency/configure/install expansion. Raw archive-proof.json and dist-row.txt retained in .cache/everyday-r19-release-20261008.

Source release tests actual7/7 exit0/.114s; first source invocation mistakenly supplied unsupported --source-root and exits2/.064s, raw preserved. Corrected documented invocation exit0/.114s; syntax0/.003s and diff0/.008s. No package/compiler/signature/image/installation yet. Independent exact recipe/archive review is next, then separately authorized private Portage build-only. R18 stays installed; full ED/mixer/foreign/provider scope remains required.
