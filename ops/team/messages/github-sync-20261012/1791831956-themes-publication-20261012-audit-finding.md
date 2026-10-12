# Public readiness audit: material findings

- Time: 2026-10-12T19:05:56+00:00
- License decisions needed: QindaBoot `a49f02407d1170c574921fe6bd2c8f9455faff4f` and QindaLLM `977f93f57fa6d2fc15fba9707dedecc012065cf2` have no source declaration superseding overlay `all-rights-reserved`. QindaDeck `2edcd922508b78314fec07b7077dcaf05679e75b` on master has license notices only in vendored Gradle/Espressif files, not a project-wide declaration.
- Source declarations: LocalAiPortage 117 GPL-3.0-or-later headers, LlamaWatch 53, JevReader 10. Guide code/text LGPL-3.0-or-later plus GPL showcase media provenance. Web main is documentation-only without a license; its active feature branch has COPYING and 37 GPL-3.0-or-later headers.
- Completed clear-license scans: FoxAgent, guide, LocalAiPortage, IconArt, LlamaWatch, Photocraft and both Web refs have no credential-pattern matches and no blobs over 100 MiB. Guide `.openai/hosting.json` contains only project_id/static metadata, no credential field; hosted Site remains owner-private independently of public source. Its sample archive names and provenance describe fictional Signal Garden documents.
- Photocraft gitlink points to public upstream `storytold/photocraft`, commit `decc7715b956ef6c227dd6506379d54b46f341f9`; scan scope is this repository's reachable objects, not the separate upstream history.
- KWin complete reachable-history scan remains active at low CPU/I/O priority; no test, build, model, service or inference work ran.
