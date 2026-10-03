# Temporary native provider handover — existing installed procedure

- Exact source:dec748147853ca12859c53dd8a3a3ee40fc17ece.
- Independent source-only reviewer:qinda_icon_brand_audit. Root owns all actual package/provider/session/dialog actions.
- Claimed scope: own board/new reply only; no implementation, queries of providers/user data, runtime, UI, install, build or GPU.
- Root reports imports laptop3/12 and qinda3/67. Complete both actual idempotence/unchanged ciphertext+catalog gates before this handover; qinda retry and final hash results are not inferred.

1. Root retains original stores and signed rollback packages. Recheck the manager-selected old GNOME unique owner/UID/PID, open its pidfd and send only graceful SIGTERM; wait for that exact owner to disappear. No SIGKILL. Keep its existing activation path held for the selected handover so it cannot race to reacquire secrets. This procedure does not retire KWallet or alter current physical applications.
2. Use actual installed /usr binaries after the final package gate, not source-build copies. Existing nested names/socket must be free; preserve the physical compositor/apps. Parent isqindaqt-0 on laptop and the existing named software Waypipe socket on qinda. Do not unshare PID or substitute a private bus.
3. Keep HOME actual and set XDG_DATA_HOME=$HOME/.local/share. Private CONFIG/CACHE/STATE directories are fine with no-autostart. Do NOT pass daemon storage-root/runtime-root/private-bus/policy-fixture: main.cpp66 would disable normal ResidentLockPolicy when those root flags are explicit. Default data location opens the actual imported ~/.local/share/qindaqt/keyring.
4. Launch the existing actual nested compositor/direct supervisor with native keyring enabled, using this argv/environment shape. Prepare task-private0700 config/cache/state directories first; all substitutions are non-secret path/display facts.

```sh
env -u DISPLAY -u WAYLAND_SOCKET -u LD_LIBRARY_PATH \
 DBUS_SESSION_BUS_ADDRESS="unix:path=/run/user/$(id -u)/bus" \
 XDG_RUNTIME_DIR="/run/user/$(id -u)" \
 XDG_DATA_HOME="$HOME/.local/share" \
 XDG_CONFIG_HOME="$taskqa/config" XDG_CACHE_HOME="$taskqa/cache" \
 XDG_STATE_HOME="$taskqa/state" QT_QPA_PLATFORM=wayland \
 QT_QUICK_BACKEND=software KWIN_COMPOSE=Q LIBGL_ALWAYS_SOFTWARE=1 \
 QT_QPA_PLATFORMTHEME= \
 /usr/bin/qindaqt-kwin --wayland-display "$parent_display" \
 --socket qindaqt-4090 --width 1024 --height 768 --no-global-shortcuts \
 --exit-with-session '/usr/bin/qindaqt-session --notification-host /usr/bin/qindaqt-notification-host --shell /usr/bin/qindaqt-shell --no-portal --no-autostart --native-power off --no-polkit-agent --no-powerdevil --no-global-shortcut-daemon --no-input-method-daemon --no-removable-media --network-secret-agent= --desktop-controls= --night-light= --welcome='
```

Do not add no-keyring or no-lockscreen. Supervisor main.cpp164–166 selects installed keyring; KeyringSessionLifetime creates a dedicated same-UID connection and invokes AttachSessionWithDisplay(native4090). The actual first owner is pinned; SessionDisplayBinding validates real native compositor/socket/peer lineage. The --wayland-display parent flag makes witnessed activation scope Private, suppressing host activation-environment publication/resident refresh; it is not a fake private Secret Service bus.

5. Confirm the actual new org.freedesktop.secrets/org.qindaqt.Keyring1 owner/PID is the native child and that its supervisor display attachment/NativeLock policy is genuine. Then reuse existing test_secret_service.py377–385 ordinary libsecret operations: secret-tool store with a unique controlled qindaqt-cutover-probe attribute and synthetic bytes on stdin; lookup that same attribute, capture/compare only; clear it; final lookup must be empty. Capture stdout in memory and report only success/equality/cleanup. Never lookup a user's imported secret or emit plaintext. SecretService.cpp137–138 admits ordinary same-UID clients; those clients need no native-session attestation of their own. The resident's actual attachment governs prompts and native policy.

The controlled probe can advance native collection metadata/ciphertext even after deletion. Therefore original snapshot hash/idempotence checks come FIRST; do not call this a no-write verification or require post-probe bytes to equal pre-import files. Existing imported data preservation is already proved by the prior exact retry; this step verifies actual standard libsecret store/lookup/clear compatibility.

## Lifetime and honest stopping point

Keep the temporary nested compositor, supervisor and qinda relay alive while that provider is the current owner. Parent socket loss, supervisor bus-owner loss or compositor teardown retires attachment/lock policy; supervisor stop also stops its keyring child and sends Shutdown. Closing the test window therefore cannot leave a qualified durable provider running independently.

For failure/teardown, root sends graceful termination to only its exact owned nested process, waits a bounded interval, and restores the retained old provider if it is still responsible for the physical session. Do not silently leave the current apps with no Secret Service owner. Successful temporary handover can remain until deliberate fresh native physical login. Stop the temporary owner before the new supervisor's first-owner attachment; the old attachment cannot simply be reassigned.

No fresh physical login is required for this real same-UID standard Secret Service check. Fresh native physical login remains required to claim physical-session integration and native PAM automatic unlock. Existing apps may need to reopen standard Secret Service sessions after provider-owner change; do not claim every cached old client recovered from this procedure or force-close them. This is a temporary real provider route, not physical/PAM acceptance.
