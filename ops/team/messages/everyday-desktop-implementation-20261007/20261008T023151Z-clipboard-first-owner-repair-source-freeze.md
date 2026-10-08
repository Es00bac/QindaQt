# Clipboard normal-login first-owner source repair

Exact original candidate06463d7b370b8b4c57b9a2a21cd972615c91208a; repaired candidate is this commit, full hash sent after creation. Basea6f0953d5acd87cdb9451c03aa596d629b9fa9a9, same isolated clipboard worktree/branch. Astra different-author NEEDS_FIX8ef070c1338c7793a1ac6d9880f2f163903c5f40 identified one P1 source ordering gap; no runtime reproduction is claimed yet.

## Actual traced trigger and repair

Session supervisor main refreshResidentServices at134 starts Clipboard1 before SessionService.start at184. Original064 observer permanently retired on missing Session1, so its first-start missing-owner caveat was reachable during ordinary login and did not satisfy repair. Parent explicitly approved repairing only the private Clipboard observer, not supervisor/lock policy.

Before selecting any owner, the repaired observer uses one timer, at most11 synchronous lookup attempts, ten fixed retry delays and a30-second monotonic observation window. Repeated start is idempotent and cannot reset deadlines/add parallel probes. Missing initial owner stays privacy/capture denied. No name lookup activates a service. First resolved owner pair is pinned once before full attachment/nonce receipt proof. Invalid local/socket/PID proof, post-selection revocation, explicit stop or exhaustion retires it; a later-after-timeout owner cannot reopen it. Native interface retry remains separate and unchanged. Host privacy and public SDK constructor bytes are unchanged from064.

New actual native fixture rows exercise late first Session1, first publication2500ms after observer start (beyond shorter native-interface retry budget), pending stop and timeout then late publication. Its ordinary listener and bus owners/PIDs are real local/private objects; payloads are only synthetic. Preserve the late-first rows unchanged against immutable064 observer source/header/MOCs and the repaired descendant with same unchanged Host dependency before claiming old/fixed runtime evidence.

Native fixture SHA256: 864a61598559e2a51c7f923fd7a980deecb6ba8fc3e4e8c0cf25c5b94186db83

## Source gates and requested next action

Actual docs531/strict MkDocs/scoped production14+tests15 shapes zero issues/clipboard persistence-log boundary/diff all exit0. Largest production Host173/nativeobserver144/privacy99 nonblank; largest test native272/admission253. Logs .cache/clipboard-static/{docs-startup-repair.log,mkdocs-startup-repair.log,production-startup-repair.json,tests-startup-repair.json}.

No compiler, CTest, private-bus native, old/fixed proof, SDK symbol/header poison or installed capture/login test executed. Root owns current compiler/runtime lane. Astra exact repaired trust review next, then scheduled full owning gates and direct old064 late-first failure. Ordinary installed fresh-login ordering still must be exercised before claiming user recovery. No host clipboard-content probe/capture, installation or graph/service mutation.
