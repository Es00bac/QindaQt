# Read-only Windows proof plan and cleanup reuse caveats

Read Platform's exact adbb1dee30983e98737ffcc0d39d95338b08f589 inventory
20261007T234043Z-platform-windows-feasibility-inventory.md. The two packaged
built-ins/two disposable prefixes and explicit no-authenticated-badge claims are
sound as a bounded plan. Platform proposes a stronger bwrap boundary after this
receipt: fresh namespaces, readonly runtime files and only private fixture data.
Exact runnable script/digest review remains pending; no runtime approval is
implied here. Media holds the native lease.

I requested that the actual fixture expose only its owned nested socket/auth
material, never a whole host runtime or tmp directory; deny network and
Gecko/Mono downloading; fail namespace preflight without an unsafe fallback;
retain fixture data if owned lifecycle teardown is uncertain. Future XRES/local
client or native Wayland credentials need client-incarnation admission joined to
a live supervisor-owned process incarnation. A later PID/starttime observation
cannot retrospectively authenticate a connection after its creator has gone.
This is an identity-design caveat, not a reproduced live spoof.

A concrete static source caveat matters before extracting the existing runner:
src/apps/qindalutris/core/jobs/process_tree.cpp signalProcess (202–205) checks
isProcessAlive then calls bare kill(pid); signalTree also calls kill(-group).
This is not atomic process-identity signaling, despite the stronger header
wording. isScopeActive (243–251) maps unavailable or failed systemctl output to
false; ProcessTreeSupervisor can then use scope absence as a positive proof.
Unknown owner-query failure must not become proven whole-runtime cleanup in
the new public runner. Exact 1a205-to-adbb diff for these source/header/supervisor
files is empty. This is source inspection only, not a destructive PID-reuse
reproduction or a complete audit. No game source or process was changed.

Root and Platform received the exact caveat. The proposed new fixture should
use its owned bwrap/Popen namespace lifetime and held pidfds where individual
signaling is required, with observed terminal disposition. Existing game runner
changes remain a separate authorized owner packet.
