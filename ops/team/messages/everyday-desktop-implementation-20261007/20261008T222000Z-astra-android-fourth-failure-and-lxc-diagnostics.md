# Fourth Android VM: retained failure and narrow diagnostic successor

The separately granted fourth attempt at plan e97ee8f5652475479ebcab29c34fc248353cb62cadd1768fe55a6442910f9fcc failed. No app windows or Android milestone accepted.

Actual outer2560607/start11483599 was reaped/absent; unit main2560612/start11483604, invocation f0e17a93a6d04bd7a1dc3cfc41ae6b4e. Unit1/23.423s; observer23.576961s. QEMU2560733/start11483923 retired0; inputVersionsStable and innerCleanupQualified true. Six live cap witnesses confirm4CPU/5GiB/swap0/tasks256/affinity0-7/nice10. Laptop heavy released.

Stock init, binderfs and LXC configuration completed; both read-only ext4 images mounted; nested QindaQt accepted android-proof Wayland socket; UID1000 windows driver ran. Waydroid then waited10seconds for LXC RUNNING and raised OSError container failed to start. Final observed STOPPED and image unmounts qualify cleanup, not successful startup.

Immutable own .cache/android-fourth-vm-proof.tar.gz:20784bytes/SHA256d973d5a71c81342ebe90dabb4e7bf01fa87e4bf30c3c053b9bcaf3a23e7b572f,10members/9indexed payloads independently rehashed on qinda. Includes original argv/plan, immediate active/first-live witnesses, full serial/unit, result/dispatch. Earlier failures and archives unchanged.

## Causal logging gap and smallest source change

Installed pinned Waydroid1.6.3 helpers/lxc.py401-404 launches lxc-start -P /var/lib/waydroid/lxc -F -n waydroid -- /init through run_core.background. run_core.py44-50 captures both streams and logs DEBUG. helpers/logging.py16-21 filters DEBUG from console unless details_to_stdout; lines74-80 normally retain it in /var/lib/waydroid/waydroid.log. The guest container-start argv omitted that flag, although init and stop already used it. That volatile guest log is unavailable after VM retirement; no claim identifying cgroup/namespaces/binder/rootfs cause can be made.

Root authorized a one-line guest-only correction: add the global --details-to-stdout before container start. Installed parser helpers/arguments.py149 accepts it, stdout_logger now forwards DEBUG at existing root DEBUG level. No package/source runtime change, log-tail reader, policy, permission, timeout or success relaxation. AST and exact one-line diff inspected. Next gate: root review fresh one-object archive plan, then separately granted fifth VM.

Serial also exposes missing XKB data, cursor theme, UTF8 locale, font cache and PipeWire support plugin. Those need selected-input qualification for full usability but are not established causes of LXC failure. No blanket stage expansion or retry performed.
