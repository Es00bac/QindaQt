# Audio/OBS causal findings before implementation

Base `01e919f6` has four bounded groups of defects, now recorded in the owning
wiki pages before source changes: authentication close codes discarded and
top-bar requests not tracked; Settings console delegates replaced during a
drag and all console operation results ignored; virtual-bus module destroy
events untracked; OBS bridge replaying the last Audio1 owner's wiring after
owner loss. Existing OBS profile/first-run discovery and ordinary audio-row
drag fixes remain intact.

The monitor node-name mapping was checked against the upstream OBS
PulseAudio capture implementation and is consistent. No live host evidence,
compilation, configure check or runtime test is claimed. Next: bounded source
corrections in assigned modules, followed by a committed uncompiled candidate
for the root's integrated build.
