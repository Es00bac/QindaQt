# Platform source review — ACCEPT

- Candidate: e3a1396a06869242e375fedba909fee206c0fc35
- Reviewer: Everyday Review Sol
- Verdict: ACCEPT
- Time: 2026-10-07T17:25:18+00:00

Independent review checked the exact candidate before tests and did not edit product paths. ADR-0345 already accepts installed native-power-exclusive OFF while retaining the exclusive fail-closed implementation and later physical receipt gate. Requiring the native lock PAM package follows the existing fixed-service prerequisite/ADR-0349 and strengthens delivery completeness without changing authentication trust. Current r6 fork commit/tree and exact 6.6.6.1 ABI agree with the manifest, bare fork hub and immutable r6 recipe; additive receipts preserve historical ADR/source pins. Retired Plasma exclusions and native dependency/provider guards remain enforced.

Independent gates on candidate source:

- `python3 tools/test-check-release-contract`: exit 0; 7/7 unittest cases.
- `tools/check-release-contract --desktop-ebuild .../qindaqt-desktop-0.1.0_pre20261002-r15.ebuild`: exit 0; exact ABI/fork contract.
- Temporary independent mutations of staged r15: exit 0; 20/20 rejected (all 12 retired Plasma runtimes, missing lock-PAM/UPower/BlueZ/portal/native-power/exact-fork/provider and exclusive ON).
- `compositor/tools/verify-kwin-source`: exit 0; fork 24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae; bare hub `rev-parse COMMIT^{tree}` returns 3a257b8d991777247970c83ce9ae8ca265448db3.
- `mkdocs build --strict --site-dir build/ed-review-site`: exit 0.
- `tools/validate-docs`: exit 0; 514 Markdown documents/navigation verified.

Caveats: staged r15 recipe is mutable and was used only as a policy fixture, not accepted as a final release recipe. Final immutable source pin/archive, package metadata/build/signature, installed closure and fresh physical login/unlock/native-power parity remain separate gates. Compiler/private-runtime lease stays with Files worker; no compiler, installation, live service or physical action was performed.

Requested next action: integrate this exact source candidate and rerun affected source/doc gates; send exact Network installation and final overlay r15 candidates for independent review. Reviewer is waiting, not working, until a candidate arrives.
