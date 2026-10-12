# ACCEPT: signed Office r3 artifact, build ID 1

- Timestamp: 2026-10-12T17:05:52+00:00
- Reviewer: printing-review-20261012
- Overlay recipe: `9f6eed509cfe027bced4e1b00b4ef1a15d4e35be`
- Office source: `d47c4d905534bea0b2d65394eebf35dc5b7bbab5`
- Artifact on qinda: `/var/cache/binpkgs/.qa-office-printing-r3-20261012/gui-apps/qindaoffice/qindaoffice-0.1.0_p20261010-r3-1.gpkg.tar`
- Build ID: `1`
- Bytes: `43,294,720`
- SHA256: `3e4a6a7891000f6922d4113b3e6ffebc3c93dd3670dc2048783c767219ce7eab`
- Verdict: ACCEPT this exact binary artifact for manager adoption through Portage.

Independent review invoked Portage GPkg with required signatures and `_verify_binpkg(metadata_only=False)`, checked the full artifact digest, and matched the embedded recipe SHA256 `4f7e8f6aa59572253af23a0e678a3bcc93855757e40a95c35912d66ad5452df3` against the previously accepted local recipe. The embedded exact source pin is d47c4d9. Both DEPEND and RDEPEND contain Qt CUPS; only RDEPEND contains qindaqt-printing.

Reviewer independently extracted the signed payload into a separate temporary image, counted 68 regular files and zero symlinks, matched every path/byte count/mode/SHA256 to the author's file ledger, confirmed qindawrite exists, and passed all ten ELF dynamic dependency checks. The temporary review image was automatically removed. Full verifier assertions exited 0; no rebuild or installation occurred.

The retained Portage build result records buildpkgonly exit 0 (16:50:41–17:01:03 UTC), exact recipe candidate and matching source archive hash. Independent final read-only checks confirm qinda still has Office r2 installed, world hash unchanged from build preflight, and configured MAKEOPTS unchanged at -j24 -l24.

Manager owns publishing/adoption and installed-tree verification. Printing target activation r1 is separately accepted at 422c0248519c47d225a716c52864bf7a0f64e20c. Manager's owner-confirmed production Write job 8 qualifies physical output separately; this artifact review itself made no printer or desktop action.

All requested exact reviews are complete. Concrete help offer: focused rereview of a demonstrated installed artifact or package adoption defect, preserving source/worktree ownership and without repeating the completed full build.
