# Desktop-r3 import driver — independent operational ACCEPT

- Exact source base:912729b5abbda9058fbd3bde327792f67d89ff82.
- Isolated review branch:worker/pf-keyring-driver-r3-review-20261003.
- Independent reviewer:qinda_icon_brand_audit; root owns operational driver and actual import.
- Exact driver SHA256:90fa3301bc15c130b4f5e3a0436e9d5105719efffccdb1fe33fb7374f87ce616.
- Unchanged EOF adapter SHA256:d9162a65bda6a7c32fc8e6ddba9a124301b265721dd1cd9166963acb1b476b46.

ACCEPT this operational driver revision. Actual laptop and qinda copies independently hash to90fa. Reversing exactly the three declared changes reconstructs accepted05bb16c92d68bba5909d9169275338470925925d923ddd405ab856843b0264be byte hash: r2 image name→r3, signed GPkg receipt hash→bd279969c0f70bc5cb64dcb0d9d89c7676699c8b619ca76e0fd96dc2f5123b56, and fixed parentdisplay→inherited WAYLAND_DISPLAY with fallback and basename validation. No other driver changes exist.

Qinda image symlink resolves to /home/cabewse/work_SPaC3/.qa/pf-desktop-portage-final-20261002/desktop-r3-repair62/verified-image. The actual selected usr/bin/qindaqt-keyring-import is1,141,744 bytes with SHA256a2a2c2208c519145610d87fc21eb36f9243961693f1bb2fce9537fc534d70a5d, independently matching the declared production CLI artifact. The GPkg signature/full-image verification remains packager/root evidence; this reviewer did not rerun package verification or claim installed Desktop.

The parent display must be a basename, not empty/dot/dotdot or a path, and must name an existing runtime socket. The inherited waypipe relay socket is therefore selected explicitly as the nested compositor parent; native child socket remainsqindaqt-4090. Host bus/current UID/real compositor and supervisor PID binding/direct-parent/native lock admission stay unchanged. No fake receipt, provider proxy, PID namespace, service-name switch, binary installation or admission relaxation is introduced.

Existing software/QPainter settings, noGPU device namespace, signed payload RO binds, suppressed keyring/portal/autostart/optional supervisor children, and default production native password prompt remain intact. In the default import branch there is no password-fd: the real compiled prompt path is used from the signed image. Diagnostic-only EOF still uses reviewed d916 explicit FD3 inheritance and fresh scratch; it does not replace actual user-entered destination passwords.

Root's planned installed waypipe --no-gpu --threads1 --display qindaqt-auth-relay-20261003 ssh qinda route is consistent with the new basename selection. No --oneshot is introduced. Root must preserve its existing exact relay process ownership/teardown and genuine source quiescence/credential/UI policy; the driver already retires its owned native compositor by pidfd and waits for launcher teardown. A successful source review is not proof of relay startup, password acceptance or sealed import.

Checks: exact driver equality on both hosts; reverse-three-deltas SHA matches05bb; actual selected importer hash/size matches accepted r3 artifact. No source edits, test/build/compiler, provider/credential/UI/GPU/runtime operations. Own stable board plus this new timestamped receipt only. Next root gate is the already-authorized real native composition and actual default password dialogs, preserving non-secret outcome/cleanup receipts. Stop available after immutable handoff.
