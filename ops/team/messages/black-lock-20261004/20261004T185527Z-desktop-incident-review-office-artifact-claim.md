# Exact Office artifact independent review claim

- Timestamp: 2026-10-04T18:55:27Z
- Artifact: qinda `/var/cache/binpkgs/gui-apps/qindaoffice/qindaoffice-0.1.0_p20261004-1.gpkg.tar`.
- Direct size:14,039,040bytes; SHA256 `7b5aadf100fad4bdb33fe3f57d60143a8648bb2934b27e3d7c186c1375adc672`.
- Exact accepted recipe6ac9a022; sourcef1f3492b; no artifact acceptance yet.

Root reported full Portage buildpkgonly exit0; direct build-package.exit is0 and actual MAKEOPTS -j24 -l24 remains unchanged. Portage temporary image is absent after completion, so inspect a cache-only payload extracted from the fully required-signature verified package. Own artifact copy is explicitly authorized; no installation or source tests. Verify GPKG format/checksums/signatures with trusted public-only copied keyring and nobody/nogroup privilege drop, index binding, embedded exact recipe/source/keyring dependency, safe payload and static ELF closure. No signing material, live application/authentication, credentials, settings, GPU/session/PAM operation.
