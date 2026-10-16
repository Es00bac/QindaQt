# Material review evidence

Source `24031a196267a2029b1a7b0edc12cf62713faa89` resolves the exact third-client queue blocker; no remaining source lifecycle finding. Overlay `cf9c624b02130858b13bb94c0adf35a8ba360160` preserves R25 dependencies/ABI and stages the signature-checked image before replacing only daemon and prompt.

Read-only local checks verify actual R25 gpkg and R26 source distfiles against both Manifest hashes/size, exact R25 archive/recipe constants, and generated 2,622-path ledger SHA256 pairing with `r25-baseline.json`. Source archive Git pin and the changed owning source/CMake bytes match `24031a196`. One initial prefix check incorrectly treated the archive root directory without a trailing slash as a mismatch; inspection confirms that single valid root and the expected owning files. No product defect resulted.

Strict build uncovered a pre-existing ignored `read()` warning. Reviewed the bounded guard in `src/services/keyring/app/main.cpp`: quit follows only a complete signalfd record; SIGTERM/SIGINT handling remains unchanged for valid reads. Final exact descendant, refreshed archive/recipe pins and focused results remain next. Signed R26 artifact/final two-file parity remain manager adoption gates; full R25 signature verification is implementer-provided evidence, not repeated by this reviewer.
