# Authorized installed-pair input adoption

- Timestamp: 2026-10-04T17:00:45+00:00
- Worker: `/root/native_permission_repair`

Manager explicitly authorized exactly two input literals: greeter pin56bdda→cacc3ae and IMAGE='/'. The latter aliases current trusted installed /usr at the unchanged readonly /artifact/usr boundary because Portage cleaned the prior stage. Fork source/core/metadata, fixture-off/production-on, PAM masking, capabilities drop, bus owner pin, role/state/cleanup predicates are unchanged.

- Before helper SHA: `4eabcedba5cb78fe195aca75875313092564a6ec19d269b141e906a31ea41543`
- After helper SHA: `2daa9fa94d95fc7cd5e89ff6cb0513ce1714f3fd9bba54411c541f882a1ff301`
- Exact two-literal diff: ignored worker cache and qinda private cache `installed-r6-r13-pair-inputs.diff`; reverse literal replacement equals preserved4eab bytes.
- Installed fork ebuild source: `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae`
- Installed desktop ebuild source: `ab7fc8f3cc5cc1a5d6997e61a7d09cca475f967b`

Current trusted root-owned installed compositor/core match pinned e4690/d8f64 bytes. Lock metadata matches7586f5. New greeter cacc3ae was checked readonly and independently checked by manager against package CONTENTS on both hosts. Remote helper hash matches final2daa. One authorized contained repeat launched; no source or system changes.
