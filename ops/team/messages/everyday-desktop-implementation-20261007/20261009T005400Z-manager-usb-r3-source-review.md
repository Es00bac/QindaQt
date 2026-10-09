# Manager review: Removable Media R3 device export

- Time: 2026-10-09T00:54:00+00:00
- Exact desktop source candidate: 5266d4d1dd4ddd093777e6f314ca2bf50e32a516
- Exact QindaGentoo recipe candidate: 4b5861c244a5eda9fcc8f37cdfa04dbd16669da8
- Decision: ACCEPT source/recipe for one component Portage build, not installed USB completion.

The live laptop has a mounted USB volume and File Manager R23 contains
MediaDeviceSection/Eject, but installed gui-apps/qindaqt-removable-media-r2
pins older source 5bff539e and exports only Activate. File Manager's public
MediaClient therefore sees no /Devices snapshot. Source5266 adds a bounded
synthetic self-owner Version/GetSnapshot check and truthful obsolete-owner
recovery wording, without changing the real UDisks action authority. The
probe never registers the production well-known name, reaches system UDisks
or submits Eject. R3's exact Git archive/Manifest pin preserves r2 and
requires desktopR23; its libexec path matches the installed fork.

Owner evidence: corrected reduced configure passed10.7s; production helper
compiled in the warm continuation17.87s and synthetic /Devices smoke passed
in0.04s. The first cold build stopped at120s; optional standalone test
compilation later stopped at36/40 and no Qt test rows ran. Preserve this
coverage gap; one Portage package build and installed image check follow.
The user's real USB drive was not ejected or modified. A fresh login with
R3 then needs a read-only File Manager sidebar inventory check and a
user-directed Eject interaction for physical acceptance.
