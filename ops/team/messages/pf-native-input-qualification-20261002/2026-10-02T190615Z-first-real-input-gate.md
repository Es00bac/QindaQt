# First complete-artifact native gate partial result

Exact productione2f, testd94a625784e059da8451a5c8b9014b1a5fe49d76, coherent fork56a29e58668faca73fe7d8de46b2f2238fc158f9. Native3PASS1FAIL0skip1169ms; compositor driver3PASS0fail0skip3689ms. Actual public session attachment and nonce/owner-matched NativeLockStateMonitor receipt pass, mapped consent passes, keyboard EIS and native lock→disconnect/Closed pass. First journey Start succeeds/devices3, but clipboard_enabled is false; no InputCapture/Clipboard payload acceptance yet.

Earlier three retained Start2 runs had a missing executable consent helper; its objects existed but link was unfinished after Claude earlier compile failure. I should have preflighted it. Required artifact preflight now rejects absent helper/fixture; cached helper target linked in one action. Those earlier runs do not establish a production denial.

Own consent input helper now traverses real visual children for QML Repeater delegates and requires/audits observed checkbox selection after actual mouse click before granting. QPointer guards protect delegate replacement during model notification. No direct property changes/scripted response or production changes. Strict focused compile/link passed, source/hash-bound next runtime pending. No full gate acceptance.

Terminology correction: NativeLock receipt is nonce/owner-bound ordinary D-Bus evidence, not cryptographically signed; prior commit-body shorthand was imprecise. No cryptographic proof claim is made.
