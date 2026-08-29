# Audio1 integrated installed lifecycle gate: PASS

- Verifier: Codex Audio1 installed lifecycle verifier
- Time: 2026-08-27T16:56:32-06:00
- Decision: **PASS**
- Exact integrated commit: `fac2756a65572f37296c0fb6bd38b74aa68574d3`
- Exact integrated tree: `2f129e0efdaa9a559a8b36b185c8866a4c53d4ec`
- Production build directory (read-only): `build/audio1-integrated-production`
- Staged install root (read-only): `build/audio1-integrated-stage/usr`
- No configure, build, compiler, install, product/source edit, or host-session interaction was performed.

## Installed artifact preflight

The verifier required the pinned HEAD/tree and rejected every tracked change
except the manager-owned, concurrently edited `docs/HANDOFF.md` and
`docs/TASK_LIST.md`. The manager confirmed those two bookkeeping edits; the
verifier neither read nor edited them. There were zero unexpected tracked
changes.

All **18/18** Audio1 paths appeared exactly once in the production install
manifest and existed in the stage:

- 3 static libraries: protocol, client, and service;
- 11 public headers: 4 protocol, 3 client, and 4 service;
- the installed `qindaqt-audio-service` executable;
- the D-Bus activation descriptor, hardened systemd user unit, and Audio1
  introspection XML.

The three staged libraries byte-matched their production build products. All
11 staged public headers byte-matched their source headers. The staged D-Bus
descriptor and systemd unit byte-matched their generated production files; the
staged XML byte-matched its source. Both descriptor `Exec=` and unit
`ExecStart=` resolved to the exact staged executable. The installed executable
is an executable 64-bit PIE, has build ID
`88a68e8ef482c8d28071f86070de397f826de565` matching the production build
product, and has installed RUNPATH `$ORIGIN:$ORIGIN/../lib`.

Key staged hashes:

- executable: `d3630f9393e34822c460aa24923d04f5f13cb9900569812a68aedb9b1c7deeca`
- protocol library: `7cc48f0dd0f22f153c144a7d9519a1585e946ec23e28bf2375567d539e6a0bd8`
- client library: `e27e1b547686d08bbf044100ef5bf85537e6c1d46e75fa9a8e4bfeab63e45dbe`
- service library: `35bce6ae59bda166a60dbe398c5c8b132a8e7022c9055217a5e3786fc50c0061`
- D-Bus descriptor: `eb8f4998ce782425afc2b622fc856b0b90fbcfa5f7670f04559a38aa8b6bb282`
- systemd unit: `22db39b77e7d82e541faa27f5cb4b0e38e9302f824f6dc1801a302d0ddb50b87`
- introspection XML: `aabc33c6851ac4f828fc538a20f1b16dcfc679f5c5003739ef3fe4ce80f29854`

The ignored helper and complete ignored log are:

- `build/audio1-installed-lifecycle-gate.sh`
- `build/audio1-installed-lifecycle-gate.log`
- final log SHA-256:
  `a310a16d372816ce6651ec8afea1e827dc7e80fb81477b3d075f1091fbb9411b`

## Containment

Every daemon was launched with `env -i`. Each phase had verifier-owned private
`HOME`, `TMPDIR`, `XDG_RUNTIME_DIR`, `XDG_CONFIG_HOME`, `XDG_CACHE_HOME`,
`XDG_STATE_HOME`, `XDG_DATA_HOME`, and `PIPEWIRE_RUNTIME_DIR` directories.
`XDG_DATA_DIRS` named only the staged share directory used for activation.
Each bus had an explicit private Unix socket under a disposable short `/tmp`
root. `DBUS_SESSION_BUS_ADDRESS` named that socket. `PIPEWIRE_REMOTE` named a
deliberately nonexistent remote inside the empty private PipeWire runtime.
`DISPLAY`, `WAYLAND_DISPLAY`, and all host session variables were absent.

Only the private bus address was passed to `gdbus`. The replacement bus held
one additional private `gdbus monitor` connection before activation so its
textual unique owner could not coincide with the first bus owner. All cleanup
signals were guarded by exact `/proc/<pid>/exe` identity. No host session bus,
audio graph/device, desktop, input, or user configuration was used or
inspected.

## Exact command and lifecycle result

```sh
bash -n build/audio1-installed-lifecycle-gate.sh
bash -o pipefail -c 'bash build/audio1-installed-lifecycle-gate.sh 2>&1 | tee build/audio1-installed-lifecycle-gate.log'
```

Exit status was 0. Each of the 20 phases required:

1. successful `org.freedesktop.DBus.StartServiceByName` with reply
   `(uint32 1,)`;
2. `GetNameOwner` resolving `org.qindaqt.Audio1` to an exact unique owner;
3. `GetConnectionUnixProcessID` on that unique owner;
4. canonical `/proc/<pid>/exe` equality to the exact staged executable;
5. a successful `GetSnapshot` sent to the unique owner, with protocol version
   1 and a parsed nonzero epoch;
6. termination of the exact constructing private `dbus-daemon`; and
7. bounded proof that the exact service PID disappeared.

Exact per-cycle service evidence (`first PID/epoch -> replacement PID/epoch`;
owners were `:1.1 -> :1.2` in every cycle):

| Cycle | First PID / epoch | Replacement PID / epoch |
| --- | --- | --- |
| 1 | `243955` / `7950339576989452362` | `244024` / `310044982999472945` |
| 2 | `244076` / `1412479024632700389` | `244130` / `6160500826024194217` |
| 3 | `244181` / `7276193554903516163` | `244235` / `884799666380546705` |
| 4 | `244301` / `2539653538498451024` | `244355` / `56455026292593287` |
| 5 | `244406` / `448774567899146394` | `244460` / `2205686085060960999` |
| 6 | `244513` / `4584162814466501075` | `244568` / `577997425330792500` |
| 7 | `244619` / `4720791924156758730` | `244673` / `1654322536113951836` |
| 8 | `244724` / `7223093908257756214` | `244778` / `1181685744700680624` |
| 9 | `244829` / `2149477547823156671` | `244883` / `1916403034943754530` |
| 10 | `244934` / `3057403832160852243` | `244988` / `9217952213555039266` |

Final gate line:

```text
SUMMARY cycles=10/10 activations=20/20 exits=20/20 replacements=10/10 exact_pid_absence=20/20 exact_service_scan=0 fixture_roots=0
```

An independent post-run log/residue audit found 20 activation records (10
first, 10 replacement), 20 exit records, 10 replacement records, 20 unique
service PIDs, 20 nonzero parsed epochs, and 20 successful StartServiceByName
replies. Every recorded PID was absent. A canonical `/proc/*/exe` scan found
zero live exact staged service processes, and the verifier `/tmp` fixture-root
scan found zero roots.

## Pre-gate verifier corrections

These did not contribute to the accepted 10-cycle count. The first attempt
stopped before any activation because the private Unix socket path under the
long manager directory exceeded the kernel limit; its trap left zero service
or fixture residue. A diagnostic rerun completed one valid cycle but then
waited on the intentionally persistent private owner-reservation monitor. It
was interrupted, its exact cleanup again left zero residue, the monitor
shutdown was bounded and exact-executable-guarded, and the accepted run was
then restarted from cycle zero. These were verifier-only issues, not product
failures; daemon startup stderr is now copied into the persistent ignored gate
log on any such pre-cycle failure.

## Bounded caveats and requested action

This gate accepts only installed Audio1 packaging and private D-Bus activation,
constructing-bus loss, replacement-process/authority/epoch freshness, and exact
exit cleanup at the pinned integrated commit/tree. It intentionally exercised
an unreachable private PipeWire runtime. It does not qualify physical USB,
HDMI, Bluetooth, jack, multichannel, microphone/speaker, hotplug,
suspend/resume, realtime latency, hardware gain mapping, CPU/PSS budgets, or
future Audio Settings/shell UI.

Manager may record the integrated installed Audio1 lifecycle gate as passed:
**10/10 cycles, 20/20 activations/exits, zero residue**.
