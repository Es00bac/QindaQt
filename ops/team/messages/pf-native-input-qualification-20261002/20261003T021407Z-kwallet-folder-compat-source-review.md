# KWallet folder compatibility — independent SOURCE ACCEPT

- Candidate: 62acb69b931a091a9ee5161cf65dba570b5a17a4.
- Parent: 5917da5c054ccd8ed9fd0be5a2ac9db549a4bb1c.
- Reviewer: qinda_icon_brand_audit, different from legacy_reader_compat implementer.
- Explicit qinda bare-hub branch fetched before reading; no GitHub push.
- Scope: exact six-path source/test/wiki diff and unchanged encrypted verifier. No build, test execution, provider call, secret read, UI or GPU.

## Verdict

SOURCE ACCEPT. The observed root trace stopped after folderList and real public metadata found repeated folder identities. This candidate changes only that exact compatibility boundary.

DuplicateText defaults to Reject. Only initial and recheck KWallet folderList call sites opt into Collapse. Wallet names, entry keys and all Secret Service paths retain strict uniqueness. text decoding and actual-owner/signature checks remain intact.

The codec counts every raw array string before decoding/deduplication using observed++ against maximum. With1024 allowed, the1025th element still returns Capacity even when all names repeat. Repeated responses cannot bypass raw bounds. Distinct folders are sorted and compared on the same identity-set basis at recheck; an actual added/removed folder still refuses. Duplicate multiplicity changes alone are intentionally irrelevant because each identical public API folder selector refers to the same folder.

The data copy path and planner are unchanged. Each distinct folder is visited once per complete acquisition pass; repeated keys across different folders retain distinct tuple-derived item identities. Password/stream/map bytes and derived portal records are not rewritten, filtered or decoded.

## Fixture review

New duplicate-folder success requires2 collections/5 items and invokes the unchanged encrypted verifier. That verifier checks authenticated storage, all original binary payloads/types/folder attributes and the derived portal item. Test call counts require8 readEntry and8 entryType calls for4 source entries ×2 full passes,4 ×foldercount entryList checks, and balanced openAsync/close.

The1025-entry raw repeat fixture requires Capacity2 before any readEntry and no output or destination. Separate duplicated-wallet, duplicated-entry and Secret Service collection-path cases require InvalidInput1 without destination publication. Unique-folder mutation despite duplicate responses requires Conflict3 with no output/destination. Existing successful both-source/idempotence and per-provider completeness assertions remain unchanged.

The normative import wiki now documents folder identity normalization, raw1024 limit, unchanged other uniqueness checks and no source edit. No public/process/persistence boundary changed, so a new ADR is not needed.

## Checks and remaining gate

git diff --check5917→62ac exited0. Static call-site inspection confirmed the two Collapse sites and default Reject on every other texts call. No runtime or focused fixture result is claimed by reviewer.

Next action: manager accepts exact source and packager executes the existing focused compile/private keyring_legacy_import fixture. Actual repaired import must use signed Portage delivery and genuine native session; this source review grants no deployment or migration claim.
