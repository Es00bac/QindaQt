# Stock Waydroid source-provenance review help

- Time: 2026-10-08T17:25:46Z
- Candidate remains f67653a03ddb98f958d536f5a4d777d512c36fee.
- Source/read-only help only; no runtime, package, VM, mount or service execution.

Platform requested the actual source behind the direct container CLI PID/bus-owner and block-device assumptions. Laptop installed app-containers/waydroid-1.6.3 VDB CONTENTS matches all six actual files below by its recorded MD5. This establishes installed-package consistency, not an independent upstream cryptographic signature claim.

| Installed path under /usr/lib/waydroid | SHA256 |
| --- | --- |
| tools/__init__.py | 35987b9640e2e107ea496ccb4ffe1eee48dc063ad3a9ce5ecd9189c9689dd4a8 |
| tools/actions/container_manager.py | d6dc808e20e2b0cb4c295117d0bdac2b39e2f4f8480206f56467c776c24b463e |
| tools/actions/initializer.py | 91260aaef3189be0f6ab4860b600fa8a58e4f398c70499b3eb7d32d734cd2c77 |
| tools/helpers/lxc.py | 081cbbde2ec09da29707168a1504bd10716482ce5c3069a3f46281a69db27e27 |
| tools/helpers/mount.py | 67483f7553240793ed909d667188c4f48468d29298dd171cb70f94503ed85511 |
| data/scripts/waydroid-net.sh | 6648976dfb065c6a923b587d029030a5e9dee59b9dfec5eb35a84528602580f3 |

initializer.py lines56–57 accepts block devices through stat.S_ISBLK(os.stat(...).st_mode). Its identical qinda cache is everyday-android-images-20261007/.cache/android-image-inventory/initializer.py.

container_manager.py start lines107–127 creates its GLib main loop, constructs the initializer/container objects on SystemBus, acquires id.waydro.Container with do_not_queue, then runs the same loop. The inspected start function has no daemon fork. Actual private bus owner PID equality remains a required runtime observation, not inferred from this trace.

The installed preserved ebuild invokes make install with USE_NFTABLES=1 and USE_SYSTEMD=1. No official1.6.3 archive was found in the bounded cache searches and Makefile was not inspected; these are explicit provenance limits, not substituted claims. Matching kernel module closure, runtime packages, prepared root and generated boot image remain separate admission gates.

No source changes or new runtime qualification. Requested next action: Platform completes exact f676 source review; root separately routes qualified package/input construction.
