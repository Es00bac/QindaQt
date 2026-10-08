# Stock Android runtime source claim

- Base: 37f1be1a9297ccf7f79d6c08daa586227c90710b.
- Owned: tools/foreign-runtime/android-stock-proof/ (new test-only area), focused tests/local README, own board and replies. No shared registry/wiki edit yet.
- Accepted inputs: da5591db2961a0f50a07a21dfd448648100cc6f6 / 29cbc1021e5c52ea5528fef7db7b87fd3cbdbe4d; independent99b92233.
- Source-only boundary: no init, module load, mount, guest, VM, namespace, compiler, package or service action.
- Concrete constraint: stock LXC retains sys_time/sys_admin; stock image helper loop-mounts ext4 and stock initializer probes Binder/modules. Root mount/net/PID isolation alone is insufficient. A VM with a private kernel contains these operations. Qinda has QEMU/KVM, matching Portage kernel with Binder built-in, dracut and virtio-9p modules; stock runtime/LXC remain package prerequisites.
- Next: manager review of smallest boot fixture using only read-only public package trees, guest-owned tmpfs state, no host sockets/devices except QEMU KVM. Existing root-only machine-image inventory remains unknown.
