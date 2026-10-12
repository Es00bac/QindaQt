# ACCEPT: Office r3 recipe 9f6eed509cfe027bced4e1b00b4ef1a15d4e35be

- Timestamp: 2026-10-12T16:50:48+00:00
- Reviewer: printing-review-20261012
- Exact overlay candidate: `9f6eed509cfe027bced4e1b00b4ef1a15d4e35be`
- Base: `39d26063991f9713133f0661ff5db3fd2b2a2ab1`
- Source: `d47c4d905534bea0b2d65394eebf35dc5b7bbab5`
- Verdict: ACCEPT recipe/source archive for one Portage build-only package. Final binary artifact and installed behavior remain separate gates.

The three-path delta adds r3 without rewriting the r2 recipe/archive, appends the new Manifest row and advances the delivery Office row. The recipe pins the independently accepted source and verifies its .dist-commit during prepare. Qt CUPS support is added to build/runtime dependencies; the accepted printing stack is appended to RDEPEND after DEPEND captures the compile closure, so build-only does not pull runtime printer services. The remaining recipe behavior is unchanged.

Independent checks: exact clean HEAD; Bash syntax and diff-check exit 0; reviewed r2/r3 delta. Verified local archive byte count 8,240,160, Manifest BLAKE2B/SHA512 and SHA256 `23d62cc4602bd1f856a18f3db3cd5a66415dba6593bd1a509714d39725a4e97e`. Opened its 3,839-entry single-prefix inventory, accepted .dist-commit marker, vendored QXlsx CMake source and absence of .git metadata. Compared all four key modified print implementation/UI files directly with git show at the accepted Office source; exact matches.

Author's independent qinda/laptop make-dist runs matched and its final build-only pretend selects one package with zero new source downloads. Recipe clearance sent to printing_defaults, who owns the single configured-MAKEOPTS build on qinda. Reviewer will inspect final package metadata, source pin, signature and image inventory when available without recompiling or installing it.
