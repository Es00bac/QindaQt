# Android authenticated producer and Portage prerequisites — help offer

Source-only help requested by root after copier qualification; no producer code,
runtime/image/APK installation, service activation or private-display test.

## Stock seam and concrete missing producer

Read the installed 1.6.3 host tools/interfaces/IPlatform.py: its thirteen
transactions cover properties, app list/info/install/remove/launch/name,
settings and launchIntent. It returns display/component metadata, not
authenticated signer/UID/task/window/surface association.

Re-fetched exact pinned upstream source text into ignored android-producer-help
cache. The [HWC mode parser](https://github.com/waydroid/android_hardware_waydroid/blob/6e898e9d18f442873305f4992df6e6148aa1e693/hwcomposer/modes/waydroid_mode.cpp)
extracts package/task from layer-name text. The
[framework patch](https://github.com/waydroid/android_vendor_waydroid/blob/1b95b85221f4faaa357932fa5e93eacb7430f636/waydroid-patches/base-patches-33/frameworks/base/0007-wm-Include-task-id-in-surface-name-for-easier-tracki.patch)
prepends a real task ID to WindowStateAnimator's app-supplied title.
[wayland-hwc.cpp](https://github.com/waydroid/android_hardware_waydroid/blob/6e898e9d18f442873305f4992df6e6148aa1e693/hwcomposer/wayland-hwc.cpp)
publishes the derived app ID on the Wayland toplevel. These sources support
the existing static spoofing finding; they are not proof of the as-yet-uninspected
image binary or a live exploit.

Proposed smallest trustworthy producer, **inference/design, not existing API**:
a versioned guest system_server ownership endpoint using PackageManager
package/UID/signer facts and WindowManager/ActivityTaskManager task/window
incarnations; a protected typed association carried through SurfaceFlinger/HWC
to the actual exported wl_surface; and a host broker/compositor admission
endpoint tied to the admitted HWC connection plus current runtime boot/session.
The surface binding must name the actual protocol resource/incarnation, not
reconstructed object IDs, title, package string or host renderer PID alone.
Guest Binder/SELinux admission must reject ordinary app-created association
records. Task handoff, child surface teardown, package update and runtime restart
retire mappings before reuse. Shared-UID/independent-stop uncertainty stays
unsupported. A host-only IPlatform wrapper or desktop-file generator is
insufficient. No new producer or privileged protocol is implemented here.

## Package inputs and safe first proof

The inspected ::guru waydroid-1.6.3 ebuild owns runtime helpers/units but its
pkg_config calls ordinary init, whose image path otherwise downloads software.
Installed initializer.py recognizes a preinstalled system.img/vendor.img pair
under /usr/share/waydroid-extra/images and sets both image OTA fields to None.
Platform can package that pair as fixed licensed/Manifest-verified data, with
a separate bounded recipe for each selected built-in APK after actual image
package/version/ABI/signing inventory. No store or ad-hoc APK/image download
qualifies. Pin and package the guest framework/HWC producer patch set and host
helper/units too; preserve mutable user data outside package-owned payloads.
The runtime GPL license is not proof of all image component redistribution terms.

Existing 1.6.3 ebuild prerequisites include binder IPC/binderfs, memfd, LXC
seccomp, PSI, nftables/NAT, gbinder and Wayland/audio dependencies. The leased
runtime proof must establish actual binder nodes/mounts, namespace/network
ownership and render access; kernel config or package presence alone is
insufficient. Keep stock global container/binder/network resources reserved;
a private nested Wayland socket alone does not isolate a second runtime.

Offer: after Platform has exact image/recipe inputs, I can review its producer
identity schema and spoof/restart/child-window negative packet. First real
proof should use two licensed packaged built-in apps with recorded package
and surface evidence. Android two-app proof and authenticated green chrome
remain open; no physical session change was made by this help packet.

Source text SHA256: mode.cpp
5baf43b67d40c28a86cfc0ae2cecd9660f5c813e3cdea75649a15461e1dd9747;
task-name.patch
34e94059f23229c469bde0620c5341a458f439de5a3ddee46ceccfbc0dcd5dbd.
