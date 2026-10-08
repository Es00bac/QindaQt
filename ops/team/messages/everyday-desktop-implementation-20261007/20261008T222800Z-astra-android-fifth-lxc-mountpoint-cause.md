# Fifth Android failure: stock LXC mountpoint absent

Exact once-only5e84/6cf2 attempt settled1/23.565396s. Outer2576944/start11537059 reaped; main2576948/start11537064; invocationa7ccfc4a226b4bd78898e3fc95e785fd. QEMU2577044/start11537341 retired0/20.647s; input versions stable and cleanup qualified. Six actual4CPU5GiB/swap0/tasks256/affinity0-7/nice10 witnesses. Heavy released; no app windows.

Immutable .cache/android-fifth-vm-proof.tar.gz21549bytes/SHAd2eeac4c0860778b2299fa41ff68f3547936641e53916bb07cb640e3f8b08b52;10members/9payloads independently rehashed on qinda.

Actual serial522–524 identifies lxc_storage_prepare ENOENT on /var/lib/lxc/rootfs, followed by rootfs-init/pinning failure. Both Android image and overlay mounts had succeeded. Stock post-stop /dev/null hook126 is secondary and unchanged.

Installed app-containers/lxc-7.0.0 ebuild122 configures -Drootfs-mount-path=/var/lib/lxc/rootfs. Its CONTENTS116–118 owns /var/lib/lxc/rootfs and README stating it must exist as the private-namespace temporary mountpoint. Waydroid config_base3 independently sets lxc.rootfs.path=/var/lib/waydroid/rootfs. Official https://raw.githubusercontent.com/lxc/lxc/v7.0.0/src/lxc/conf.c lxc_storage_prepare checks access(rootfs->mount,F_OK) before storage_init; this matches the actual failure. Fresh guest /var tmpfs intentionally hides installed package /var directories.

Root authorized minimal guest-only creation of /var/lib/lxc/rootfs with parents0755 under existing022 mask, before stock init. No host directory/config change or new package input. Actual extracted mkdir AST executed against disposable rebased temporary root: fresh creation, all four parent/leaf modes0755, existing-directory repeat pass; full AST passes. All package/archive originals remain unchanged. Request exact one-object archive then separately admitted sixth VM; no rerun performed.
