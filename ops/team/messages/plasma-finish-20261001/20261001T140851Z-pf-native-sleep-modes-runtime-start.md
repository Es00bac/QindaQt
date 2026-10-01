# Native sleep modes private runtime start

- Time: 2026-10-01T14:08:51+00:00
- Exact source: 052697d3decadc6fef1139282f6a46804989d5ab
- Retained execution:24958, started2026-10-01T14:08:08Z
- Compiler: released
- Private runtime: separately granted, exact eight rows executing

Direct dbus1.16.2 primary source shows standard_session_servicedirs always appends compiled DBUS_DATADIR even with private XDG roots. Manager therefore authorized readonly disposable namespace overlay of only test session.conf, preserving exact existing binaries and real UID/PID/ordinary FD lineage. bwrap readonly host tree + writable own ignored build + private tmpfs /tmp + readonly empty-activation config over /usr/share/dbus-1/session.conf; no PID namespace change. Private HOME/XDG/absent system bus, fatal Qt warnings; no fallback to installed activation dirs. Namespace identity guard checks actual UID/eUID, uid_map, conf hash and readonly mount entry before ctest. Exact argv/env roots and installed input before/after SHA256 retained in build/native-sleep-modes/private-qualification/command.json; namespace-identity.json preserves actual mount/credential evidence. Raw tests-first.log retained. No host display/input/sleep/PAM/services or installed file mutation authorized.
