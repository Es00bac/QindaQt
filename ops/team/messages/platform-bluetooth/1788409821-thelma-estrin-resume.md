# Thelma Estrin — QQ-005.05 B1 replacement resume

Resumed the preserved `f081a86` WIP after the prior provider outage on exact
base `ce9228d9694622d503d92a38d01986f8f124f188`. I retained the direct-QtDBus
design, made ADR-0056 explicitly supersede only ADR-0037's BluezQt library
choice, and preserved ADR-0037's BlueZ pairing/trust authority.

Material findings and repairs so far:

- the fake ObjectManager used an invalid hand-marshalled nested value and did
  not route child object paths; it now registers exact QtDBus container types
  on one private-bus virtual subtree;
- concurrent discovery acquisitions previously sent one StartDiscovery per
  caller; they now share one in-flight BlueZ call and retain caller-scoped
  bounded references;
- invalid-bus initial truth is queued so the backend generation is installed
  before publication, strict property types fail closed, duplicate device
  addresses retain the lexicographically first path, and late owner replies
  remain fenced;
- the 738-nonblank production source was decomposed; the source-shape gate is
  green and the largest owned production source is 499 nonblank lines;
- a B1 component-install boundary row now proves the exact public installed
  surface. The 14 focused/adjacent Bluetooth rows pass in Debug and Release.

One pre-existing adjacent row, `qindaqt.bluetooth-staged-install`, attempts a
whole-repository install and fails in this focused build at the first unrelated
unbuilt archive (`libqindaqt_profiles.a`). Building the default repository is
explicitly forbidden by the lane contract, and that script is outside this
lane's ownership; the handoff will report this bounded evidence caveat.
