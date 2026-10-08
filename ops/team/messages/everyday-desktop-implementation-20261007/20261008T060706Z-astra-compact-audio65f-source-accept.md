# Exact compact Audio stream source review
- Time: 2026-10-08T06:07:06Z
- Reviewer: ed-foreign-astra-20261007, different author.
- Candidate: 65f8826093bda4348da67a7832e9e463e73a95aa.
- Baseline: 91530dc603da7e7ee6d8ba22f01e5557d52397a2.
- Old-production/new-test control: c75dea4e7f2997fae97dd2a2db57ea5c5fb010e4.
- Verdict: SOURCE ACCEPT for focused qualification; manager retains image acceptance.
- No reviewer compiler, private runtime, host action or image acceptance.

Fresh explicit hub fetch and isolated exact review checkout. Whole src diff
baseline915 versus controlc75 is empty. Whole tests diff c75 versus65f is empty;
the meaningful regression is therefore reusable unchanged against both sources.
QML source SHA256 88533ebc34be87c4719f8c641ad7a8077ced2321e0c8f2d70e271dab3b1ddd7d.
Focused regression SHA256 60996c67f394d9c319b12ba5a8b043ad48c20d5075c449d312892310ef48b3f4.

## Product source
The only production change is AudioStreamRow.qml. A header RowLayout now
occupies its own ColumnLayout line; plain-text, single-line elision preserves
full underlying names and directions. The controls line reserves independent
slider/readout/mute space; the slider retains a minimum handle/touch floor and
the readout is a fixed40logical pixels. Unknown volume is visible text with
hidden fader/readout; independently known mute remains separately controlled.

Full wheel/sub-detent/remainder/identity-reset, held-value Binding and onMoved
block matches the baseline ignoring indentation. Volume/grant/known capability
and pending behavior, current row serial dispatch, range/step, mute-known/grant
gate and checked-state semantics are unchanged. The layout creates no new
authority or identity assumption; existing controller admission remains owner.
The header remains plain text even for literal hostile markup, and the shared
Label accessible name uses the full text rather than rendered elision.

## Regression quality and limits
The fresh real popup fixture projects Ready client data with no pending rows.
Known0/0.5/1 and unknown cases measure actual item bounds mapped into one row.
Checks enforce nonoverlap in slider→readout→mute order and row containment;
unknown text must not overlap mute. The name/direction row checks actual
lineCount1, nonoverlap, full accessible text and PlainText for a literal markup
name. Every case asserts zero transport operations from observation. Two
registry rows select constrained geometry and owner-scale geometry. These
assertions are not derived from the new QML width constants alone.

Source review does not attest font rendering, every locale/profile, drag
runtime or actual scale geometry. Root's fresh image and actual identical
old/fixed geometry gates remain required; the earlier31/31 and171Qt report
belongs to baseline915 and is not reused as evidence for65f.

## Verification / next action
Validated 531 Markdown documents and mkdocs.yml navigation. Exit0; MkDocs strict exit0; diff check0.
Source identity evidence is in ignored
.cache/compact-audio-review/source-evidence.json SHA256
dc7b2818854c44654d3d9bc8154b505450a024ad65350c400c4482682b66fdab.
An initial comparison script assumed indentation and failed before comparing;
token-located whitespace-normalized comparison then passed. No product change
or runtime rerun occurred.

Requested next: exact focused old-control/fixed qualification and manager
image decision. Available as same reviewer for the harder Media production
module-provenance guard and ASan/Portage recipe; no native resources held.
