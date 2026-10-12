# Handoff: newer project public-source readiness

- Time: 2026-10-12T19:11:26+00:00
- Role: read-only audit; root owns publication and licensing decisions.
- Results: no credential-pattern findings and no Git blobs over 100 MiB in the scopes below. Three project-wide license decisions remain. `public-ready` means source/license/readiness audit, not runtime product certification.
- Detailed safe metadata: `qinda:/tmp/qinda-public-readiness-20261012.json` (paths/types/counts/object IDs only; no secret values).

| Project and exact reviewed ref | License evidence | Public-ready and concrete blocker |
| --- | --- | --- |
| LocalAiPortage main `7d9bde75b722081a09d59ae0dadd0f176f1da392` | 117 GPL-3.0-or-later SPDX declarations, including CMakeLists.txt and helper/source files; overlay app-portage/qindaportage GPL-3+ | Yes. Only fixture logs under tests/data were artifact candidates. |
| QindaBoot main `a49f02407d1170c574921fe6bd2c8f9455faff4f` | No source SPDX/LICENSE/COPYING declaration; overlay sys-boot/qinda-boot all-rights-reserved | No. Explicit open-source licensing decision required; do not infer a new license. |
| QindaFoxAgent main `9316a5aecf0eb75105cece809eb372eea27a859e` | Root LICENSE is MIT | Yes. No source-audit blockers. |
| QindaIconArt main `b4862f05bb87e05c41bd3b31d7c09302db701519` | README declares distribution LGPL-3.0-or-later; retained sources/COPYING-ICONS, COPYING.LIB, GPL-3.0.txt and modification notices; overlay LGPL-3+ | Yes. About 873 MB of unique historical raw blobs, largest 8,821,525 bytes; no individual-size blocker. Artifact candidates are provenance validation logs. |
| QindaLLM main `977f93f57fa6d2fc15fba9707dedecc012065cf2` | No source SPDX/LICENSE/COPYING declaration; overlay sci-ml/qinda-llm-server all-rights-reserved | No. Explicit open-source licensing decision required. No model/runtime operation was invoked. |
| QindaLlamaWatch main `756d28976e57d8a0bcab2239eecb35f400645e72` | 53 GPL-3.0-or-later SPDX declarations, including CMakeLists.txt and app sources; overlay gui-apps/qindallamawatch GPL-3+ | Yes. No source-audit blockers. |
| QindaPhotocraft main `9ae48802ef91fb5b7d4450dbcdbced79a7c96c27` | Root LICENSE/native headers MIT; native Rust channels MIT OR Apache-2.0; existing overlay retains dependency license list | Yes for wrapper repository. Gitlink third_party/photocraft points to public storytold/photocraft commit decc7715b956ef6c227dd6506379d54b46f341f9; separate upstream history is outside this audit. |
| QindaTK-agent-guide main `3d515432d65fedffe8ce047c455e4316c092063b` | README/source headers LGPL-3.0-or-later; dist/sources.md retains GPL-3.0-or-later showcase-media provenance and says documents are fictional | Yes for source publication. Hosted Site remains owner-private; .openai/hosting.json has only project_id/static metadata, no credential fields. Fictional sample archive paths inspected. |
| QindaWeb main `1c7ee2ffbb20eac05b5fe5832c6879ba3bc3c2df`; feature/native-browser `2def7d9e2252d33a839041bdc2b4b1461ba1521d` | Main is documentation-only without a license declaration; active feature has COPYING and 37 GPL-3.0-or-later SPDX headers; overlay www-client/qindaweb GPL-3+ | Feature tip: yes. Main alone is not the implemented/browser source publication target. Preserve actual branch status and avoid claiming feature integration from README-only main. |
| qindaqt-kwin main `24d0c6a6fedc68256a20a480bc3fe8b5d871a1ae` | Upstream README GPLv2+; LICENSES and file-level GPL/LGPL/BSD/MIT/CC0 notices retained; overlay gui-wm/qindaqt-kwin GPL-2+ | Yes within scoped credential audit below; full-history size gate passed. |
| JevReader main `843cae73e43a31d084edc809d9c67ae58ddf358c` | 10 GPL-3.0-or-later source headers, including jevreader modules, despite no root LICENSE file | Yes. Related optional developer tooling: selective source/document reader and MCP server for local coding agents, with Jev requests delegated to qinda; prototype, not a desktop app or model runtime. |
| QindaDeck master `2edcd922508b78314fec07b7077dcaf05679e75b` | No project-wide license declaration; only vendored Gradle wrappers and Espressif source files carry their own notices | No. An explicit project-wide open-source license is required; vendored notices do not cover all authored code. |

## Executed source audit

Read only Git objects and metadata; invoked none of the projects' code. Full reachable default history was scanned for LocalAiPortage (26 commits/288 blobs), Boot (2/46), FoxAgent (4/86), IconArt (75/11,033), LLM (6/19), LlamaWatch (11/107), Photocraft (5/56), guide (2/46), JevReader (2/19), and Deck (13/160). QindaWeb scanned the union of main and the active feature tip (22 commits/127 blobs).

Checks included private-key markers, recognizable GitHub/AWS/other API tokens, plausible credential assignments/credential URLs, likely private runtime/config artifact paths, current-tree contents, and all reachable blob sizes. Literal placeholders and low-entropy examples were not promoted to secrets. There were no surviving pattern findings, so no secret values were printed or stored. These checks cannot prove the absence of every possible opaque secret or encoded private datum; no such guarantee is claimed.

KWin's full inherited-history size audit covered 30,526 commits and 188,614 unique blobs (8,518,388,836 raw bytes); largest historical blob is 897,162 bytes and none exceed 100 MiB. At manager request, stopped only the owned full-history credential scanner and its Git child after verifying documented public KDE base v6.6.6: tag object `43cb730ca363b995dfd5f0ceb537e4c37a7bb5ff`, peeled commit `9bf2235fad10de9048c634e376bf12e56b3023e6`, tree `88f96f8cde49c51552d82f60fd461b6e8b950685`. Tag, tree and ancestry agree with qindaqt/UPSTREAM.md. Completed credential/artifact scan covers every reachable object added after that base plus every blob in the current tree: 121 downstream commits, 3,725 unique blobs, 52,209,645 bytes, zero findings. Unchanged pre-base KDE history was not fully rescanned for credentials.

All final audit scripts exited 0. The intentionally stopped broad KWin scanner's SSH process exited 255; its partial work is not claimed as a completed full-history credential scan. No model calls, inference, runtime model checks, service operations, source builds, package installs, downloads, project edits, public API creation or pushes occurred in this assignment.

## Exclusions and next action

QindaHane remains explicitly proprietary and must not be public. RecoveryBackup is existing private data, QindaVBAN is empty, and floor-body-150 is an out-of-scope hardware design. Games were not audited or published.

Root can publish clear-license exact tips while obtaining one explicit licensing decision for Boot, LLM and Deck. Overlay coverage owner has the source evidence, clear FoxAgent/guide audit, and JevReader GPL header correction. Available for bounded follow-up on exact new license commits or audit findings; no further product work claimed.
