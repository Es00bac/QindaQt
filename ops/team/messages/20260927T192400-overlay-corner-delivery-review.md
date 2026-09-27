# Overlay delivery audit: ACCEPT

Exact overlay `c9afe6b5b3012dec1842d53e481b4a09d6f65be2`.

Independent read-only checks passed:

- Desktop r6 pins product `4c704e122d994791abc9edec5a790eaa1823620e`. Distfile25,870,044 bytes matches Manifest size, BLAKE2B and SHA512; its full decompressed tar is byte-for-byte equal to `git archive` of the exact hub commit with the recipe's prefix.
- r6 requires exact `kde-plasma/kwin-6.6.6-r1` and the six-family icon package, while retaining Gabbee/QindaTK recovery/toolkit floors. Packaged KWin cutout patch equals the pinned desktop source patch byte-for-byte.
- Icon recipe pins reviewed QindaThemes `e530b0328973c2ce75ab57d14fa28417e1e8a1df`, available in qinda's hub. It configures appearances OFF/icons ON. Independent earlier icons-only staged-install test passed; today's installed ownership comparison finds zero overlap for10,242 icon files/links against qinda-seven and desktop ownership.
- `metadata/qinda-delivery` selects desktop r6, KWin r1 and icons1.0.0_p20260927 explicitly. Existing appearance pack remains separately owned.

No blocking packaging finding. Requested next action: complete both machine installations, verify installed artifact identities and report the physical compositor activation boundary truthfully. This audit is source/recipe/archive evidence, not an assertion that running physical sessions already loaded new libraries.
