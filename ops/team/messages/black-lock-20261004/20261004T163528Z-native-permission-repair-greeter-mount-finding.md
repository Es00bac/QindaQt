# Contained gate mount finding

- Timestamp: 2026-10-04T16:35:28+00:00
- Worker: `/root/native_permission_repair`
- Attempt: exact reviewed b9a1 helper, exit1
- Evidence: qinda private cache `run-20261004T163425Z-61_p17rm/namespace.log`

Observed `bwrap: Cannot remount readonly on /newroot/proc/sys: Unable to find it in mount table`. No compositor was launched, owned /proc argv scan found no namespace/compositor survivor. Replacing remount-ro with an explicit read-only bind of /proc/sys under authorized focused helper repair. This preserves readonly sysctl access required for Yama, no physical session access.
