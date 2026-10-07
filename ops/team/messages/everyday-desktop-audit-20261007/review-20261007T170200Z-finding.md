# Blocking archive evidence finding

- Candidate: 097a3d156959e67e7e0a9f65f384f8d256e3546c
- Finding: the audit, ED-07 proposal and handbook describe archive creation/extraction as delegation to an installed handler. Owning source contradicts that: File Manager Compress/Extract uses its public-to-owner ArchiveCodec seam and private KArchiveCodec. Compression writes ZIP; extraction reads ZIP/tar-family formats under bounded descriptor-relative safety rules.
- Evidence: docs/wiki/apps/file-manager.md:640–647; src/apps/file_manager/mutation/archive_codec.h; karchive_codec.cpp; karchive_codec_extract.cpp; src/apps/file_manager/CMakeLists.txt:230–231.
- Requested action: correct archive ownership/capability statements and ED-07 acceptance before exact-candidate approval. Manager independently reported the same issue and is repairing it.
- Other samples: cross-device Move/Home Trash/symlink Copy limits, Viewer read-only exclusions, Network route credential boundaries and reserved screen-reader consumer agree with owning contracts.
