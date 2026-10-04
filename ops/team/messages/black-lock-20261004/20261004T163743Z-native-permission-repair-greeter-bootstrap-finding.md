# Contained bootstrap verified; temporary JSON repair

- Timestamp: 2026-10-04T16:37:43+00:00
- Worker: `/root/native_permission_repair`

Second attempt stopped before Python with /gate chdir permission denied as manager predicted. Narrow helper repair changes cwd to root-owned / and uses trusted inline `python3 -I -c` code to set no-new-privileges, disable core, drop groups/GID/UID, then exec the user-private helper. No compositor runs privileged.

Third attempt evidence under qinda cache `run-20261004T163630Z-u41e3vpa/evidence/bootstrap.json` confirms UID/GID1000, groups[], CapEff/CapPrm/CapAmb allzero, no_new_privs=true, full initial UID map. Staged compositor reached private native bus; helper then treated scalar busctl bool data as a list and failed before RequestLock. Owned compositor cleanup exit15. Temporary decoder now accepts either scalar or one-element list. Retrying; no production-source/trust/device/PAM changes.
