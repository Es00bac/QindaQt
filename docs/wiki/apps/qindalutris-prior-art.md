# QindaLutris: prior-art survey of the Linux gaming ecosystem

Research snapshot taken 2026-09-25 to inform [ADR-0275](../adr/0275-qindalutris-installs-games-and-manages-pinned-proton.md). Facts, stars, licenses and dates are as of that day; re-verify before relying on any single claim.

**Date:** 2026-09-25
**Scope:** GitHub and Linux gaming projects that make Windows games easier for non-technical people coming from Windows. For each, what QindaLutris should **learn**, **reuse** or **avoid**, measured against ADR-0275 (umu-launcher with an exact pinned Proton build, GE-Proton slots in Portage, a curated plus generated compatibility DB, store recipes, sign-in through legendary/gogdl/nile, gamescope per game, Microsoft Store unsupported).

**How the data was gathered:**
- Star counts, licenses and last-push dates come from the GitHub REST API, and latest-release dates from each repo's `releases.atom` feed. Both were pulled on 2026-09-25.
- Everything else is cited inline. Star counts are given as an order of magnitude.

---

## 1. Executive summary: top 10 recommendations, ranked by impact on "it just works"

### 1. Make every launch fully pinned and able to run offline, and store a "known-good combination" per game
**What to do:**
- Always pass an **absolute** `PROTONPATH` that points at the Portage GE-Proton slot. Never leave it unset or use a codename like `GE-Proton`.
  - Unset means UMU-Proton, which "uses the latest UMU-Proton and automatically removes old UMU-Proton builds" ([umu(1)](https://man.archlinux.org/man/umu.1.en)).
  - A codename auto-downloads the newest build ([umu.1.scd](https://raw.githubusercontent.com/Open-Wine-Components/umu-launcher/main/docs/umu.1.scd)).
- Pre-install the Steam Linux Runtime and set `UMU_RUNTIME_UPDATE=0` for launches.
- Record the working combination for each game: QindaLutris version, umu version, Proton slot, runtime version and fixes revision.

**Why:**
- A Heroic update (2.22.2) broke umu launches until a hotfix, and users had to turn umu off per game ([Heroic #5893](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/5893), [2.22.3 hotfix](https://www.linuxcompatible.org/story/heroic-games-launcher-2223-hotfix-repairs-broken-windowsonlinux-game-launches)).
- Floating updates in Lutris broke games:
  - A runner auto-download at launch corrupted the runner ([Lutris #5506](https://github.com/lutris/lutris/issues/5506)).
  - A default-runner change moved existing prefixes to a new Wine and games locked up ([Lutris #5914](https://github.com/lutris/lutris/issues/5914)).
  - After 0.5.20, users reported that no GE-Proton version worked ([Lutris forum](https://forums.lutris.net/t/lutris-no-longer-works-with-any-versions-of-ge-proton/25400)).
- Cancelling a "GE-Proton-Latest" update in Heroic deleted the old build ([Heroic #4901](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/4901)).
- umu fails with no internet or on a flaky network:
  - [umu #331](https://github.com/Open-Wine-Components/umu-launcher/issues/331) and [Lutris #6406](https://github.com/lutris/lutris/issues/6406) were closed as "invalid" and "not planned".
  - A partial runtime download shows "_v2-entry-point cannot be found" ([umu #603](https://github.com/Open-Wine-Components/umu-launcher/issues/603)).

**Comes from:** umu-launcher, Heroic, Lutris, ProtonPlus (a counter-example: its floating "Latest" entry).

### 2. Automatic snapshot before every change, plus one-click "Go back to the version that worked" (safe mode)
**What to do:**
- Snapshot the prefix automatically before any Proton-slot, fix, dependency or recipe change. At minimum, snapshot `drive_c/users` and the save folders.
- Roll back the snapshot and the known-good combination together.
- If a game exits or crashes within about 60 s of its first launch after a change, offer "Go back to the version that worked". Pruning of old snapshots is automatic.

**Why:**
- Proton warns "Prefix has an invalid version?!" when a prefix is downgraded, so rolling back the Proton slot alone is not enough ([proton script](https://github.com/GloriousEggroll/proton-ge-custom/blob/master/proton), [example](https://github.com/Vulps22/somnilux/issues/31)).
- Bottles' snapshots are manual and user-commented ([Bottles versioning](https://docs.usebottles.com/bottles/versioning.md)). A review says they eat disk space unless you manage them ([review](https://www.linuxcompatible.org/story/bottles-610-released/)), and restores have had bugs ([#2737](https://github.com/bottlesdevs/Bottles/issues/2737), [#3544](https://github.com/bottlesdevs/Bottles/issues/3544)). Copy the idea, but automate it.
- Bazzite shows a rollback tool non-technical users can handle: rpm-ostree plus `brh` plus a GUI updater ([Bazzite rollback](https://docs.bazzite.gg/Installing_and_Managing_Software/Updates_Rollbacks_and_Rebasing/), [brh](https://docs.bazzite.gg/Installing_and_Managing_Software/Updates_Rollbacks_and_Rebasing/bazzite_rollback_helper/)).
- SteamOS keeps A/B images ([SteamOS rollback](https://en.linuxadictos.com/How-to-go-back-to-a-previous-version-of-SteamOS-if-your-Steam-Deck-is-giving-you-problems-after-an-update.html)).

**Comes from:** Bottles, Bazzite, SteamOS, ChimeraOS frzr.

### 3. A pre-install verdict card that opens into a checklist, not a single score
**What to show before the Install button** (so users see it before committing):
- **Our verdict**, in CrossOver's plain five levels: *Won't install / Installs but won't run / Limited / Runs well / Runs great* ([CrossOver rating system](https://www.codeweavers.com/compatibility/rating-system)). Tie it to the exact pinned build and date tested, e.g. "Runs great on GE-Proton11-7, tested 2026-09".
- An **anti-cheat line** from AreWeAntiCheatYet (MIT), with Supported / Running / Planned / Broken / Denied ([games.json](https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/master/games.json), [license](https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/master/LICENSE)).
- The **Steam Deck / SteamOS / Steam Machine** category ([Valve compat criteria](https://partner.steamgames.com/doc/steamhardware/compat)).
- **ProtonDB** tier plus its trending tier ([summary endpoint example](https://www.protondb.com/api/v1/reports/summaries/1245620.json)).

**What it expands into:** Steam Verified-style checks: controller support, a launcher that needs a mouse, small text, anti-cheat ([steamdeck.com/verified](https://www.steamdeck.com/en/verified)).

**Precedent:**
- Heroic shows AreWeAntiCheatYet status on the game page ([Heroic Anticheat component](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/blob/main/src/frontend/components/UI/Anticheat/index.tsx)).
- ProtonUp-Qt shows Deck status, AreWeAntiCheatYet and ProtonDB for each game ([GOL](https://www.gamingonlinux.com/2022/06/proton-and-wine-updater-protonup-qt-now-shows-anti-cheat-status/)).

**Comes from:** CrossOver, Steam Verified, Heroic, ProtonUp-Qt, AreWeAntiCheatYet.

### 4. Scan the installer or game EXE offline, and explain every fix in plain language
**What to do:** when a setup file or game folder is imported, scan it offline for:
- frameworks: .NET Framework vs .NET Core, WPF, Electron, Java, Qt, XNA;
- graphics: DX9, DX11, DX12, Vulkan;
- anti-cheat folders: `EasyAntiCheat_EOS`, BattlEye;
- Denuvo and kernel drivers.

Then show *why* each fix is applied, e.g. "Installing .NET 4.8 because this game uses WPF".

**Precedent:**
- Bottles v61 added **Eagle**, which does this and names the file that triggered each recommendation ([abit.ee](https://www.abit.ee/en/soft/operating-systems/bottles-610-wine-linux-run-windows-programs-eagle-tool-executable-analysis-proton-compatibility-gami-en)).
- Bottles is GPL-3.0, so the rules can be ported. PortProtonQt already ports Bottles-sourced rules ([PortProtonQt README](https://raw.githubusercontent.com/linux-gaming-ru/PortProtonQt/main/README.md)).
- Kernel anti-cheat also has recognisable service and driver names (vgk, atvi-brynhildr, FACEIT, …) ([list](https://github.com/Sloff11550/Anti-cheat-Guide---Readme)).

**Comes from:** Bottles (Eagle), PortProtonQt.

### 5. Look fixes up in layers, and apply them before first launch

| Layer | Source | Notes |
|---|---|---|
| (a) | **umu-protonfixes** | Comes inside the pinned GE-Proton build. Look up `GAMEID`/`STORE` in the vendored **umu-database** CSV ([protonfixes README](https://raw.githubusercontent.com/Open-Wine-Components/umu-protonfixes/master/README.md), [umu-database CSV](https://raw.githubusercontent.com/Open-Wine-Components/umu-database/main/umu-database.csv)). |
| (b) | **Curated QindaLutris overrides** | Keyed by store ID. |
| (c) | **EXE-name fallback** | For titles with no store ID, as PortProton's `.ppdb` files do ([portwine_db](https://github.com/Castro-Fidel/PortWINE/tree/master/data_from_portwine/scripts/portwine_db)). |
| (d) | **ProtonDB-derived suggestions** | Only when at least 2 Gold/Platinum reports agree, the ProtonDB Badges Decky plugin's rule ([protondb-decky](https://github.com/bschelst/protondb-decky)). |

- Heroic applies its "known fixes" (winetricks verbs, programs to run, environment variables) *before* the game download starts ([known-fixes](https://github.com/Heroic-Games-Launcher/known-fixes), [GOL 2.22.2](https://www.gamingonlinux.com/2026/09/heroic-games-launcher-v2-22-2-brings-fixes-for-rockstar-ubisoft-and-more/)).
- Pinning the GE-Proton slot also pins the protonfixes, because they ship inside GE/UMU-Proton and not in Valve's Proton ([umu-protonfixes](https://github.com/Open-Wine-Components/umu-protonfixes)).

**Comes from:** umu, Heroic, PortProton, ProtonDB Badges.

### 6. Dependency installs that don't break when download links disappear
**What to do:**
- Use winetricks (LGPL-2.1) as a helper through `umu-run winetricks <verbs>` ([umu.1.scd](https://raw.githubusercontent.com/Open-Wine-Components/umu-launcher/main/docs/umu.1.scd)).
- Mirror each verb's download as a Portage distfile or on our own mirror, checked against the sha256 that winetricks already pins.
- Add our own pre-step for **dotnet48** in Proton prefixes: delete the `NDP\v4` registry keys that make the installer report the framework as already installed ([winetricks #2367](https://github.com/Winetricks/winetricks/issues/2367)).
- For GOG, let gogdl install the game's declared redistributables ([heroic-gogdl](https://github.com/Heroic-Games-Launcher/heroic-gogdl)).
- Offer Bottles-style **environment presets** (Gaming / Application) when creating a prefix ([Bottles environments](https://docs.usebottles.com/getting-started/environments.md)).

**Why:** Microsoft keeps removing installers, which breaks winetricks verbs ([#780](https://github.com/Winetricks/winetricks/issues/780), [#1405](https://github.com/Winetricks/winetricks/issues/1405), [#1845](https://github.com/Winetricks/winetricks/issues/1845)).

**Comes from:** winetricks, protontricks, heroic-gogdl, Bottles.

### 7. Store-launcher recipes that script config changes, check their own result, and can repair themselves
**What to do:**
- Every official-launcher recipe (Battle.net, EA app, Ubisoft Connect, …) writes its config changes directly and **automates** any step other launchers leave to the user.
  - Example: Lutris's Battle.net script writes `Battle.net.config` (hardware acceleration and streaming off) and sets a `locationapi` override, but then tells the user in text "Do not attempt to login… simply close it" ([Lutris battlenet API](https://lutris.net/api/installers/battlenet), [Lutris docs](https://github.com/lutris/docs/blob/master/Battle.Net.md)).
- After install, each recipe **checks** that the launcher really exists and shows Installed / Not installed / Needs repair, like Bazzite Portal's yafti entries with a status script ([Bazzite Portal](https://docs.bazzite.gg/Installing_and_Managing_Software/Bazzite_Portal/)).

**Why:** Faugus sometimes says "EA App was not installed!" after it was ([Faugus #538](https://github.com/Faugus/faugus-launcher/issues/538)), and Bottles' Battle.net installer has hung repeatedly ([Bottles #3721](https://github.com/bottlesdevs/Bottles/issues/3721)).

**Launcher picker:** put it on the "+" button, as in Faugus ([Faugus guide](https://www.downloadsource.net/how-to-install-and-use-faugus-on-bazzite/n/25869/)) and PortProton.

**Comes from:** Lutris, Faugus, Bazzite/yafti, PortProton, CrossOver's CrossTie.

### 8. Built-in store sign-in through pinned helper programs, with a fast update path
**What to do:**
- Call **legendary** (Epic), **heroic-gogdl** (GOG) and **nile** (Amazon) as separate helper processes. All three are GPL-3.0.
- Pin each version in Portage, but have a fast path for updating them when a store changes its API.
- Use Rare (Qt, driving legendary) as a code reference.

**Why:**
- The legendary repo has moved to `legendary-gl/legendary` ([legendary](https://github.com/derrod/legendary)).
- gogdl's README says it is meant to be driven by another application ([gogdl](https://github.com/Heroic-Games-Launcher/heroic-gogdl)).
- GOG changed its API in 2026, and minigalaxy versions older than 1.4.2 could no longer list or install every game ([minigalaxy](https://github.com/sharkwouter/minigalaxy)). A store change can break sign-in overnight.
- nile carries the highest risk: its last commit was in 2026-02, and games using Amazon's FuelPump may not work ([nile](https://github.com/imLinguin/nile)).
- Rare: [RareDevs/Rare](https://github.com/RareDevs/Rare).

**Comes from:** Heroic, legendary, gogdl, nile, Rare, minigalaxy.

### 9. Preflight checks with one-click fixes, and system defaults set so there is nothing to fix
**What to do:** before first launch, check:
- `vm.max_map_count` and the open-file limit;
- Vulkan and GPU driver, 32-bit libraries;
- disk space;
- controller permissions (hidraw/udev).

Then explain any problem in plain language and offer a single admin-approved (polkit) "Fix it". This is LUG-helper's Star Citizen pattern ([lug-helper](https://github.com/starcitizen-lug/lug-helper)).

Better still, ship defaults in the QindaQt system profile so these checks never fire.

**Controllers:**
- Ship Xbox controller drivers **built in or prebuilt, never compiled on the user's machine (DKMS)**, as Nobara does: xpadneo in-kernel, xone limited to the dongle ([Nobara controllers](https://wiki.nobaraproject.org/gaming/controllers/xbox), [Nobara 41](https://linuxiac.com/fedora-based-nobara-linux-41-released/)).
- Integrate **InputPlumber** (GPL-3.0+, DBus API) for remapping outside Steam ([InputPlumber](https://github.com/ShadowBlip/InputPlumber)).
- Bundle **SDL_GameControllerDB** (zlib) ([SDL_GameControllerDB](https://github.com/mdqinc/SDL_GameControllerDB)).

**Comes from:** LUG-helper, Nobara, ChimeraOS/InputPlumber, Open Gaming Collective.

### 10. Honest limits and a plain-language "Fix a problem" menu
**Per-game repair menu:**
- **Force-quit game**, **Reset launcher (keeps games & saves)**, **Show what went wrong**, **Uninstall everything**. These mirror Bazzite's `ujust fix-proton-hang` and `fix-reset-steam` ([ujust](https://docs.bazzite.gg/Installing_and_Managing_Software/ujust/)) and LUG-helper's maintenance menu.
- **Reset prefix** must warn about **Denuvo activation limits.** Heroic's own wiki tells users to wait about 24 h after recreating a prefix ([Heroic troubleshooting](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/wiki/Troubleshooting-Heroic-and-Games)).
- **Always turn umu/Proton errors into a message.** When a partial runtime download fails, GUI launchers currently show nothing ([umu #603](https://github.com/Open-Wine-Components/umu-launcher/issues/603), [Lutris #6402](https://github.com/lutris/lutris/issues/6402)).

**Titles that can't work:** show kernel-level anti-cheat, Denied anti-cheat, Wine-blocked and Microsoft Store titles honestly *before* install, with alternatives where they exist: Sober for Roblox, Greenlight or GeForce NOW cloud streaming. See §4.

**Comes from:** Bazzite, LUG-helper, Heroic, Sober/Vinegar, CrossOver ("Will Not Install").

### Also worth doing (lower impact, cheap)
- **Presets instead of raw flags.**
  - gamescope presets (Native / Upscale 720p FSR / Cap 60), **off by default on NVIDIA** ([NVIDIA HDR corruption](https://forums.developer.nvidia.com/t/display-modes-above-2560x1440p-120hz-with-hdr-enabled-cause-flickering-corruption-within-gamescope-session/295314)).
  - A "Show FPS" toggle mapped to MangoHud presets ([MangoHud](https://github.com/flightlessmango/MangoHud)).
  - Automatic power profile while a game runs, like CachyOS's `game-performance` ([CachyOS gaming](https://wiki.cachyos.org/configuration/gaming/)).
- **First-run guide and controller-first navigation**, like ProtonPlus 0.6.0 ([GOL](https://www.gamingonlinux.com/2026/08/protonplus-v0-6-0-adds-support-for-faugus-launcher-improved-controller-navigation-and-more/)), Heroic 2.22's controller mode ([Heroic 2.22.0](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/releases/tag/v2.22.0)), and Playnite's fullscreen mode ([playnite.link](https://playnite.link/)).
- **Import from other launchers** (Steam, Heroic, Lutris, Bottles, …) for people switching over, like Cartridges ([Cartridges](https://github.com/kra-mo/cartridges)).
- **Automatic cover art** from SteamGridDB, like Faugus ([GOL Faugus 2.0](https://www.gamingonlinux.com/2026/07/faugus-launcher-2-0-rolls-out-with-a-new-ui-and-many-other-enhancements/)).

### Changes this survey suggests for ADR-0275
1. **PCGamingWiki should be "link out only", not a data source.**
   - Its content is CC BY-NC-SA 3.0 ([Wikipedia](https://en.wikipedia.org/wiki/PCGamingWiki)).
   - Since the August 2026 migration, anonymous Cargo queries are refused and a bot password is required ([PCGW API](https://www.pcgamingwiki.com/wiki/PCGamingWiki:API), [vangogh #212](https://github.com/arelate/vangogh/issues/212)).
2. **Lutris installer scripts are reference data, not something we redistribute.** Community scripts carry no explicit license ([Lutris #3117](https://github.com/lutris/lutris/issues/3117)).
3. **Treat the DXVK state cache as dead.** DXVK 2.7 removed it ([DXVK 2.7](https://github.com/doitsujin/dxvk/releases/tag/v2.7)). Do not build shader-cache distribution for non-Steam games; keep a persistent per-game shader cache instead (see §3.C).
4. **Watch Xodus (Game Pass on Linux), but don't ship it.** It can sign in and download, but games do not run yet ([GOL](https://www.gamingonlinux.com/2026/08/xbox-pc-and-game-pass-coming-to-linux-with-the-xodus-project/)).

---

## 2. Reusable data and components

License compatibility is judged against a **GPL-3.0-or-later** Qt app. "Helper" means we run it as a separate process.

### 2a. Compatibility and workaround data

| Name | What | Format | License | How to integrate | Maintenance risk |
|---|---|---|---|---|---|
| **umu-database** | Maps store ID or codename to umu-id (about 2.1k rows) | CSV: `TITLE,STORE,CODENAME,UMU_ID,COMMON ACRONYM,NOTE,EXE_STRINGS` ([CSV](https://raw.githubusercontent.com/Open-Wine-Components/umu-database/main/umu-database.csv)); live API `umu.openwinecomponents.org/umu_api.php?store=&codename=` | GPL-3.0 ([repo](https://github.com/Open-Wine-Components/umu-database)) | Vendor the CSV at build time (Portage), look up offline, send missing entries upstream | Low to medium. Active, but no releases; pin a commit. |
| **umu-protonfixes** | Per-game Python fixes: winetricks verbs, environment, arguments, file tweaks | `gamefixes-{steam,umu,egs,gog}/<id>.py` ([README](https://raw.githubusercontent.com/Open-Wine-Components/umu-protonfixes/master/README.md)) | BSD-2-Clause | Comes inside the pinned GE-Proton slot; nothing extra to ship. Send curated fixes upstream, as ZOOM did ([GOL](https://www.gamingonlinux.com/2024/10/zoom-platform-store-announces-new-tool-to-run-windows-games-on-linux-with-proton/)) | Low. Moves with GE-Proton. |
| **AreWeAntiCheatYet** | Anti-cheat status for about 1,167 games | `games.json`: status, anticheats[], notes, updates, storeIds{steam,epic} ([games.json](https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/master/games.json)) | MIT ([LICENSE](https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/master/LICENSE)) | Vendor and refresh; match on Steam/Epic IDs, fuzzy-match names otherwise | Medium. Community PRs; the last `dateChanged` seen was 2026-04. |
| **ProtonDB summaries** | Tier, trending tier, confidence, report count per Steam appid | JSON: `protondb.com/api/v1/reports/summaries/<appid>.json` ([example](https://www.protondb.com/api/v1/reports/summaries/1245620.json)) | Unofficial endpoint, no terms stated | Live lookup plus cache; attribute; badge only | Medium. Undocumented and could change. |
| **ProtonDB dumps** | All reports, monthly | `reports_<date>.tar.gz` ([protondb-data](https://github.com/bdefore/protondb-data/tree/master/reports)) | ODbL + DbCL ([README](https://raw.githubusercontent.com/bdefore/protondb-data/master/README.md)) | Offline mining of repeated environment tweaks (the ≥2 agreeing reports rule); a republished derived DB must stay ODbL | Low. Monthly; latest is 2026-09-01. |
| **Steam Deck / SteamOS compatibility** | Verified / Playable / Unsupported / Unknown, plus SteamOS, Steam Machine and Steam Frame variants and test results | JSON `ajaxgetdeckappcompatibilityreport?nAppID=` ([example](https://store.steampowered.com/saleaction/ajaxgetdeckappcompatibilityreport?nAppID=1245620)) | Undocumented Valve endpoint | Live lookup plus cache only; no bulk redistribution | Medium. Undocumented. |
| **Heroic known-fixes** | Store-ID-keyed fixes (verbs, run-in-prefix, environment); about 90 Epic and 40 GOG entries | JSON ([repo](https://github.com/Heroic-Games-Launcher/known-fixes)) | **No license file** | Reference only, or ask the Heroic team | Medium; partly duplicates protonfixes. |
| **PortProton `.ppdb`** | Per-EXE-name settings: `PW_WINE_USE`, `PW_DLL_INSTALL`, `WINEDLLOVERRIDES`, `#Rating` ([example](https://raw.githubusercontent.com/Castro-Fidel/PortWINE/master/data_from_portwine/scripts/portwine_db/Cyberpunk2077.ppdb)) | Bash `export` lines | MIT (repo) | Parse as key=value, **never source**; use as the EXE-name fallback layer | Medium. Russian-language community; provenance questioned ([flathub #5195](https://github.com/flathub/flathub/issues/5195)). |
| **Lutris installers** | Community install scripts (Battle.net, EA, Ubisoft, thousands of games) | JSON wrapping YAML ([API](https://lutris.net/api/installers/battlenet), [format](https://github.com/lutris/lutris/blob/master/docs/installers.rst)) | **No explicit license** on community scripts ([#3117](https://github.com/lutris/lutris/issues/3117)) | Reference only: hand-port the facts into our recipes with credit, or get permission | High. Scripts name Wine builds that no longer exist ([#5998](https://github.com/lutris/lutris/issues/5998)). |
| **Bottles dependencies and installers repos** | Curated dependency manifests; graded (Bronze to Platinum) silent installers | YAML ([dependencies](https://github.com/bottlesdevs/dependencies), [programs](https://github.com/bottlesdevs/programs)) | No license stated in README | Reference | Medium. 83 and 183 open issues. |
| **PCGamingWiki** | Fix notes, paths, API data | MediaWiki/Cargo | **CC BY-NC-SA 3.0** | **Link out only** | High. API now requires login ([PCGW API](https://www.pcgamingwiki.com/wiki/PCGamingWiki:API)). |
| **SDL_GameControllerDB** | Controller mappings | Text | zlib | Bundle | Low. |
| **AAGL components index** (format only) | Runner catalog with a `recommended` flag, mirrors, beta channel | JSON ([components](https://github.com/an-anime-team/components)) | No license file | Copy the *schema idea* for our pinned-runner catalog | n/a |

### 2b. Helper programs and libraries

| Name | What | License | How to integrate | Maintenance risk |
|---|---|---|---|---|
| **umu-launcher** 1.4.4 (2026-07-25), about 4k stars | Runs Proton outside Steam inside the Steam Runtime; `--config` TOML; `umu-run winetricks` ([README](https://raw.githubusercontent.com/Open-Wine-Components/umu-launcher/main/README.md)) | GPL-3.0 | Portage dependency; always absolute `PROTONPATH`; runtime pre-installed; `UMU_RUNTIME_UPDATE=0` | Low to medium. Offline pitfalls (§1 #1). |
| **GE-Proton** (GE-Proton11-7, 2026-09-16), about 15k stars | Proton plus protonfixes, media codecs, patches | Mixed (Wine LGPL, various) | Pinned Portage slots (ADR-0275) | Low. |
| **proton-cachyos** | Optional "performance" Proton (NTSync, FSR4/DLSS upgrade switches) ([repo](https://github.com/CachyOS/proton-cachyos)) | BSD-3 modifications | Optional pinned slot, never the default; its own README says most users should use Steam's Proton | Medium. |
| **winetricks** 20260125 | Dependency verbs | LGPL-2.1 | Helper; mirror its downloads | Medium. Download links rot. |
| **protontricks** 1.14.1 | winetricks for Steam game prefixes ([README](https://raw.githubusercontent.com/Matoking/protontricks/master/README.md)) | GPL-3.0 | Only for Steam-client games; for umu prefixes use `umu-run winetricks` | Low. |
| **legendary** 0.21.1 (2026-09-08), about 5k stars | Epic: sign-in, install, delta patching, launch, cloud saves ([repo](https://github.com/derrod/legendary)) | GPL-3.0 | Helper | Low to medium. |
| **heroic-gogdl** v1.3.0 (2026-08-07) | GOG: sign-in, downloads, cloud saves, redistributables ([repo](https://github.com/Heroic-Games-Launcher/heroic-gogdl)) | GPL-3.0 | Helper | Low to medium. |
| **nile** v1.2.0 (2026-03-03) | Amazon Games ([repo](https://github.com/imLinguin/nile)) | GPL-3.0 | Helper | **High.** Slow cadence; FuelPump games may fail. |
| **gamescope** 3.16.30 (2026-09-24), about 5k stars | Micro-compositor: FSR, frame limit, resolution spoofing ([repo](https://github.com/ValveSoftware/gamescope)) | BSD-2-Clause | Helper per game; presets | Medium. NVIDIA HDR problems. |
| **gamescope-session** | Session framework (`gamescope-session-plus`, `CLIENTCMD`) ([repo](https://github.com/ChimeraOS/gamescope-session)) | MIT | Package it or write our own "big picture" session; **must ship a working session-select script** ([CachyOS #9](https://github.com/CachyOS/gamescope-session/issues/9)) | Medium. Last push 2026-01. |
| **MangoHud** v0.8.4 | FPS overlay and limiter; per-game `wine-<exe>.conf` ([repo](https://github.com/flightlessmango/MangoHud)) | MIT | Environment variable plus generated per-game config | Low. |
| **Goverlay** 1.9.3 | GUI for MangoHud, vkBasalt, OptiScaler ([repo](https://github.com/benjamimgois/goverlay)) | GPL-3.0 | Optional "Advanced overlay" helper | Low. |
| **InputPlumber** v0.81.0 | Input router and remapper with a DBus API ([repo](https://github.com/ShadowBlip/InputPlumber)) | GPL-3.0+ | System daemon; our UI talks to it over DBus | Low. Backed by the Open Gaming Collective ([XDA](https://www.xda-developers.com/bazzite-reveals-the-open-gaming-collective-to-make-gaming-on-linux-even-better/)). |
| **Fossilize** | Steam shader pre-cache ([repo](https://github.com/ValveSoftware/Fossilize)) | MIT | Not useful for non-Steam games; ignore | n/a |
| **EAC / BattlEye Proton runtimes** | Anti-cheat runtimes for Proton; Heroic takes them from `lutris.net/api/runtimes` ([runtimes](https://lutris.net/api/runtimes), [Heroic runtimes.ts](https://raw.githubusercontent.com/Heroic-Games-Launcher/HeroicGamesLauncher/main/src/backend/wine/runtimes/runtimes.ts)) | **Proprietary** (Valve, EAC, BattlEye) | Download to the user's machine at first need, like Heroic; **don't bundle in Portage without checking redistribution rights**. Set `PROTON_EAC_RUNTIME` / `PROTON_BATTLEYE_RUNTIME` | Medium. |
| **Vinegar** v1.9.4 | Roblox Studio via Wine ([repo](https://github.com/vinegarhq/vinegar)) | GPL-3.0 | Point to or install the Flatpak | Low. |
| **Sober** | Roblox Player through the Android build ([FAQ](https://vinegarhq.org/Sober/FAQ/index.html)) | Proprietary, Flatpak only | Point to or install from Flathub; **never redistribute** | Medium. Roblox could block it. |
| **Greenlight** v2.4.2 | xCloud and Xbox home streaming client ([repo](https://github.com/unknownskl/greenlight)) | MIT | Optional Flatpak fallback for Game Pass | Medium. |
| **Bottles Eagle rules** | EXE analysis rules | GPL-3.0 | Port the logic into our analyzer | Low. |
| **ProtonUp-Qt modules** | Steam config/VDF parsing and compatibility-tool installs, in Qt6/PySide6 ([repo](https://github.com/DavidoTek/ProtonUp-Qt)) | GPL-3.0 | Code reference or port | Low. |

---

## 3. Per-project notes

Format for each: **Status** (license, stars, last release) · **Summary** · **Copy** · **Reuse** · **Pitfalls**.

### A. Store and library launchers

#### Heroic Games Launcher
- **Status:** GPL-3.0, about 12k stars, 2.22.3 hotfix on 2026-09-16 ([repo](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher)).
- **Summary:**
  - An Electron launcher for Epic, GOG, Amazon and ZOOM, plus other games you add yourself. It includes a Wine/Proton manager.
  - umu has been the default for Proton games since 2.16 ([steamdeckhq](https://steamdeckhq.com/news/heroic-launcher-2-16-0-update/)).
  - 2.22 added a controller mode that can install and update games, plus Epic games that use Ubisoft Connect ([2.22.0](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/releases/tag/v2.22.0)).
  - 2.22.2 uses umu to provide the Steam Runtime and applies known fixes before the download starts ([GOL](https://www.gamingonlinux.com/2026/09/heroic-games-launcher-v2-22-2-brings-fixes-for-rockstar-ubisoft-and-more/)).
- **Copy:**
  - An anti-cheat badge on the game page ([component](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/blob/main/src/frontend/components/UI/Anticheat/index.tsx)).
  - EAC/BattlEye runtimes downloaded on demand ([#1513](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/1513)).
  - Fixes applied before install; runtime picked from a dropdown.
  - A warning when shader pre-caching is off under umu (2.16).
  - "Add to Steam" works even for uninstalled games.
- **Reuse:** its helpers (legendary, gogdl, nile). The known-fixes repo has no license.
- **Pitfalls:**
  - Regressions in how Heroic builds the umu command ([#5893](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/5893), [#5921](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/5921)).
  - The troubleshooting wiki asks users to run from a terminal, delete GPUCache and kill wine processes ([wiki](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/wiki/Troubleshooting-Heroic-and-Games)).
  - No Battle.net or EA support, and Electron uses a lot of memory ([shattered.io](https://shattered.io/heroic-vs-lutris/)).

#### legendary / heroic-gogdl / nile
- **Status:** legendary is GPL-3.0, about 5k stars, 0.21.1 on 2026-09-08. gogdl is GPL-3.0, v1.3.0 on 2026-08-07. nile is GPL-3.0, v1.2.0 on 2026-03-03.
- **Summary:** command-line store clients.
  - legendary: sign-in, delta patching, online-auth launch, cloud saves compatible with the Epic launcher, the Epic overlay ([legendary](https://github.com/derrod/legendary)).
  - gogdl is "meant to be used by some other application" and installs GOG Galaxy redistributables ([gogdl](https://github.com/Heroic-Games-Launcher/heroic-gogdl)).
  - nile handles Amazon, but FuelPump titles may fail ([nile](https://github.com/imLinguin/nile)).
- **Reuse:** yes, all three as helpers.
- **Pitfalls:** store API changes; nile's slow cadence.

#### Lutris (and its installer database and API)
- **Status:** client GPL-3.0, website AGPL-3.0 ([website](https://github.com/lutris/website)); about 10k stars; v0.5.22 released 2026-02-26.
- **Summary:**
  - Python/GTK game manager with many runners.
  - 0.5.20 made GE-Proton via umu the default ([GOL](https://www.gamingonlinux.com/2026/02/game-manager-lutris-v0-5-20-released-with-proton-upgrades-store-updates-and-much-more/)).
  - 0.5.21 added Valve's Sniper runtime, ShadPS4 and Xenia ([GOL](https://www.gamingonlinux.com/2026/02/lutris-v0-5-21-and-v0-5-22-arrive-with-valves-sniper-runtime-support-and-new-game-runners/)).
- **Installer API:**
  - Endpoints: `GET lutris.net/api/installers/<slug>`, `/api/games?search=`, `/api/runners`, `/api/runtimes`.
  - Each script has `files`, `installer` steps (`extract`, `wineexec`, `winetricks`, `set_regedit`, `task`…), `game`, `wine` and `system` sections ([API docs](https://github.com/lutris/website/wiki/API-Documentation), [installers.rst](https://github.com/lutris/lutris/blob/master/docs/installers.rst)).
  - Scripts are moderated by hand ([forum](https://forums.lutris.net/t/how-to-submit-installer-script-that-not-available-in-lutriss-website-database-and-something-about-what-type-of-installer-script-allowed/9322)).
  - `/api/runtimes` also exports umu, `umu-games.json.xz` and the anti-cheat runtimes ([runtimes](https://lutris.net/api/runtimes)).
- **Copy:**
  - Step types for recipes.
  - `N/A:` files that the user must supply (a setup-file prompt).
  - IGDB-backed search.
- **Reuse:** the API for live hints or reference only. There is no script license, so don't mirror the scripts.
- **Pitfalls:**
  - Scripts decay: they name Wine builds that were removed ([#5998](https://github.com/lutris/lutris/issues/5998), [#5470](https://github.com/lutris/lutris/issues/5470), [#5302](https://github.com/lutris/lutris/issues/5302)).
  - umu packaging errors: "Install umu to use Proton" appears even with a bundled umu ([#6665](https://github.com/lutris/lutris/issues/6665)).
  - Games stuck on "Launching" ([#6402](https://github.com/lutris/lutris/issues/6402)).
  - Too many settings for newcomers ([shattered.io](https://shattered.io/heroic-vs-lutris/)).

#### Rare
- **Status:** GPL-3.0, about 900 stars, 1.12.0 pre-release on 2026-08-16 ([repo](https://github.com/RareDevs/Rare)).
- **Summary:** PySide6 (Qt) front end for legendary.
- **Copy:** the Qt pattern for driving legendary, including download queues and progress parsing.
- **Reuse:** code reference.
- **Pitfalls:** Epic only.

#### minigalaxy
- **Status:** GPL-3.0, about 1k stars, 1.4.2 on 2026-07-06 ([repo](https://github.com/sharkwouter/minigalaxy)).
- **Summary:** minimal GOG client.
- **Copy:** try a **silent install first**, and fall back to the installer's own wizard if that fails.
- **Pitfalls:** a GOG API change broke versions before 1.4.2. Plan for fast helper updates.

#### Cartridges
- **Status:** GPL-3.0, about 800 stars, v2.13.1 on 2025-09-24. The README now says "no longer actively maintained" ([repo](https://github.com/kra-mo/cartridges)).
- **Copy:**
  - Import libraries from Steam, Lutris, Heroic, Bottles, itch, Legendary and others.
  - SteamGridDB covers.
  - A desktop search provider (our equivalent would be a KRunner/QindaQt search provider).
- **Avoid:** depending on it.

#### Playnite (Windows; UX reference)
- **Status:** MIT, about 14k stars, 10.60 on 2026-09-11 ([repo](https://github.com/JosefNemec/Playnite)).
- **Summary:**
  - One library covering Steam, Epic, GOG, Battle.net, Ubisoft and others, including games that aren't installed.
  - Fullscreen controller mode, local-only data, and plugins ([playnite.link](https://playnite.link/)).
  - Even metadata sources are plugins ([docs](https://api.playnite.link/docs/tutorials/extensions/metadataPlugins.html)).
- **Linux effort:** the author plans "some Linux version in 2026", desktop mode only ([GOL](https://www.gamingonlinux.com/2025/11/playnite-may-get-a-linux-version-during-2026-as-the-creator-plans-a-move-to-linux/)). None has shipped so far, and Playnite 11 has slipped ([Patreon](https://www.patreon.com/posts/playnite-10-47-146986494)).
- **Copy:**
  - The unified library, with uninstalled entitlements shown too.
  - Separate desktop and controller modes.
  - Pluggable metadata sources.

#### LaunchBox
- Linux version targeted for 2027 (proprietary) ([GOL](https://www.gamingonlinux.com/2026/08/game-launcher-launchbox-should-get-a-linux-version-in-2027/)). Market signal only.

### B. Prefix managers and umu front ends

#### Bottles
- **Status:** GPL-3.0, about 9k stars, 67.4 on 2026-09-07 ([repo](https://github.com/bottlesdevs/Bottles)).
- **Summary:**
  - GTK prefix manager: runners, environment presets, dependency manager, graded installers, versioning.
  - v61 added the Eagle EXE analyzer ([abit.ee](https://www.abit.ee/en/soft/operating-systems/bottles-610-wine-linux-run-windows-programs-eagle-tool-executable-analysis-proton-compatibility-gami-en)).
  - v66 added native umu, ProtoSoda as the default runtime, and "Adaptive Launch" ([GOL v66](https://www.gamingonlinux.com/2026/08/bottles-game-manager-v66-brings-umu-from-open-wine-components-to-help-run-games/)).
- **Copy:**
  - Gaming / Application presets ([environments](https://docs.usebottles.com/getting-started/environments.md)).
  - A named dependency list ([dependencies](https://docs.usebottles.com/bottles/dependencies.md)).
  - Installers graded Bronze to Platinum and filtered by architecture ([installers](https://docs.usebottles.com/bottles/installers.md)).
  - Versioning with one-click restore ([versioning](https://docs.usebottles.com/bottles/versioning.md)), made automatic.
  - Eagle's "why" explanations.
- **Reuse:** the GPL-3.0 logic is compatible. The dependency and installer repos state no license.
- **Pitfalls:**
  - Flatpak only: the developers "respectfully ask packagers to not package Bottles" ([Packaging wiki](https://github.com/bottlesdevs/Bottles/wiki/Packaging)). It is **not a runtime dependency for us**.
  - The sandbox often can't see other drives or folders ([#2608](https://github.com/bottlesdevs/Bottles/issues/2608)).
  - The Battle.net installer hangs ([#3721](https://github.com/bottlesdevs/Bottles/issues/3721)).
  - Bursts of hotfixes: six in 28 hours ([linuxcompatible](https://www.linuxcompatible.org/story/bottles-666-drops-six-hotfixes-in-under-28-hours)).
  - A next-generation rewrite has been announced (GOL v66), which is churn risk.

#### Faugus Launcher
- **Status:** MIT, about 2k stars, 2.4.1 on 2026-09-24; releases every two to three weeks ([repo](https://github.com/Faugus/faugus-launcher)).
- **Summary:**
  - A small launcher for Windows games built on umu, rewritten in GTK4 for 2.0 ([GOL](https://www.gamingonlinux.com/2026/07/faugus-launcher-2-0-rolls-out-with-a-new-ui-and-many-other-enhancements/)).
  - About 19k Flathub downloads a month ([Flathub](https://flathub.org/apps/io.github.Faugus.faugus-launcher)).
- **Copy:**
  - A "+" menu listing store launchers.
  - A per-game dialog split into Main / Tools / Launch.
  - Automatic SteamGridDB art.
  - Add-to-Steam.
  - Gamepad navigation.
  - A Proton manager.
  - Backup.
- **Reuse:** MIT, compatible; mainly a UX reference.
- **Pitfalls:**
  - gamescope doesn't work in the Flatpak ([#338](https://github.com/Faugus/faugus-launcher/issues/338)).
  - EA App not detected after install ([#538](https://github.com/Faugus/faugus-launcher/issues/538)).
  - No bulk import of Steam games.

#### PortProton (and PortProtonQt)
- **Status:**
  - PortWINE repo: MIT, about 600 stars, libs release 2026-08-25 ([repo](https://github.com/Castro-Fidel/PortWINE)).
  - PortProtonQt: GPL-3.0, PySide6 ([mirror](https://github.com/linux-gaming-ru/PortProtonQt)).
- **Summary:**
  - Proton/GE plus Steam Runtime wrapper, with one-click installs for Epic, Battle.net, Ubisoft, Rockstar, EA and Lesta ([overview](https://meowrch.github.io/en/usage/gaming/launchers/portproton/)).
  - PortProtonQt is the closest Qt equivalent to QindaLutris: one library, Legendary and gogdl, a virtual keyboard, and Bottles-derived compatibility rules.
- **Copy:**
  - A curated settings file per EXE, with automatic DLL/verb installs and a `#Rating`.
  - Reset a game by deleting its settings file.
  - Copy launch arguments from Windows `.lnk` files ([changelog](https://github.com/Castro-Fidel/PortWINE/blob/master/data_from_portwine/changelog_en)).
- **Reuse:** `.ppdb` data (MIT), parsed and never sourced.
- **Pitfalls:**
  - Provenance concerns about an opaque Proton binary ([flathub #5195](https://github.com/flathub/flathub/issues/5195)).
  - Mostly Russian documentation.

#### WineZGUI and WineCharm
- **Status:** WineZGUI is GPL-3.0, about 100 stars, 0.99.16 on 2026-01-15 ([repo](https://github.com/fastrizwaan/WineZGUI)). Its successor WineCharm is GTK4 ([repo](https://github.com/fastrizwaan/WineCharm)).
- **Copy:**
  - One prefix per EXE, created automatically.
  - Icon extraction to desktop shortcuts.
  - Shareable prefix-plus-game archives (`.wzt`).
  - **Prefix templates**: copy a pre-built prefix instead of rebuilding it with wineboot plus dependencies, which gives faster first installs ([ubuntuhandbook](https://ubuntuhandbook.org/index.php/2025/07/simple-app-manage-windows-apps-wine/)).
- **Pitfalls:** Bash and Zenity; a single maintainer.

#### ProtonUp-Qt
- **Status:** GPL-3.0, about 2k stars, v2.15.1 on 2026-06-24 ([repo](https://github.com/DavidoTek/ProtonUp-Qt)).
- **Summary:** Qt6 compatibility-tool manager for Steam, Lutris, Heroic and Bottles.
- **Copy:**
  - The game list shows Deck, AreWeAntiCheatYet and ProtonDB status next to each game's tool ([GOL](https://www.gamingonlinux.com/2022/06/proton-and-wine-updater-protonup-qt-now-shows-anti-cheat-status/)).
  - "Which games use this tool" before removing it.
- **Reuse:** GPL-3.0 and Qt, so the code is reusable.
- **Pitfalls:** the game list has failed to load ([#160](https://github.com/DavidoTek/ProtonUp-Qt/issues/160)).

#### ProtonPlus
- **Status:** GPL-3.0, about 2k stars, v0.6.8 on 2026-09-10 ([repo](https://github.com/Vysp3r/ProtonPlus)).
- **Copy** (from 0.6.0, [GOL](https://www.gamingonlinux.com/2026/08/protonplus-v0-6-0-adds-support-for-faugus-launcher-improved-controller-navigation-and-more/)):
  - First-run guide.
  - Controller navigation with hints and haptics.
  - Mass edit.
  - Default tool per launcher.
  - Download validation.
  - Picks the right x86-64 or ARM64 build for the CPU.
- **Avoid:** the floating "Latest" entry that updates itself ([answeroverflow](https://www.answeroverflow.com/m/1473277963398156379)). It contradicts ADR-0275.

#### Steam Tinker Launch
- **Status:** GPL-3.0, about 3k stars. **The last tagged release is v12.12 from 2023-03-14**, and the last push was 2025-12-27 ([releases](https://github.com/sonic2kk/steamtinkerlaunch/releases)). It is slowing down and close to dormant.
- **Summary:** a huge Bash/YAD Steam wrapper covering ReShade, SpecialK, mod managers, gamescope and more.
- **Copy:**
  - A GameScope settings page as a reference for flag coverage ([wiki](https://github.com/sonic2kk/steamtinkerlaunch/wiki/GameScope)).
  - Its use of the Deck compatibility endpoint ([wiki](https://github.com/sonic2kk/steamtinkerlaunch/wiki/Game-Data)).
- **Pitfalls:** needs yad ≥ 7.2 ([wiki](https://github.com/sonic2kk/steamtinkerlaunch/wiki/Yad)). A power-user tool full of knobs, the opposite of our audience.

### C. umu stack, winetricks, protontricks, compatibility data

#### umu-launcher, umu-protonfixes, umu-database
- **Status:**

| Component | License | Stars | Latest |
|---|---|---|---|
| umu-launcher | GPL-3.0 | ~4k | 1.4.4 (2026-07-25) |
| umu-protonfixes | BSD-2-Clause | — | no releases since 2018; ships inside GE/UMU-Proton |
| umu-database | GPL-3.0 | — | no releases; commit 2026-09-15 |

- **How umu resolves a game:**
  - `GAMEID` plus `STORE` pick the fix to apply.
  - `WINEPREFIX` defaults to `~/Games/umu/$GAMEID`.
  - `PROTONPATH` can be a path, a version name, or a floating codename.
  - Switches: `UMU_RUNTIME_UPDATE=0`, `UMU_NO_PROTON=1`, `--config` TOML, `umu-run winetricks` ([umu.1.scd](https://raw.githubusercontent.com/Open-Wine-Components/umu-launcher/main/docs/umu.1.scd)).
  - umu sets a per-prefix `STEAM_COMPAT_SHADER_PATH` ([FAQ](https://github.com/Open-Wine-Components/umu-launcher/wiki/Frequently-asked-questions-(FAQ))).
- **umu-database:**
  - IDs look like `umu-<steamappid>`. Store values include steam, egs, gog, battlenet, ea, amazon, humble, itchio, ubisoft, zoomplatform, umu and none ([README](https://raw.githubusercontent.com/Open-Wine-Components/umu-database/main/README.md)).
  - Live API example: `umu_api.php?store=egs&codename=Catnip` returns Borderlands 3 as umu-397540.
- **protonfixes:**
  - Uses `protontricks()`, `set_environment()` and `append_argument()`; `PROTONFIXES_DISABLE=1` turns it off ([README](https://raw.githubusercontent.com/Open-Wine-Components/umu-protonfixes/master/README.md)).
  - Example: the Elden Ring fix creates DLC files so players without the DLC avoid "Inappropriate activity detected" ([1245620.py](https://raw.githubusercontent.com/Open-Wine-Components/umu-protonfixes/master/gamefixes-steam/1245620.py)).
- **Pitfalls:**
  - Offline and poor-network behaviour (see §1 #1).
  - `UMU_NO_RUNTIME` still downloads the runtime ([#531](https://github.com/Open-Wine-Components/umu-launcher/issues/531)).

#### winetricks
- **Status:** LGPL-2.1, about 4k stars, release 20260125 ([README](https://raw.githubusercontent.com/Winetricks/winetricks/master/README.md)).
- **Reuse:** as a helper. Every download is sha256-pinned, which makes mirroring easy.
- **Pitfalls:**
  - Dead Microsoft download links (§1 #6).
  - dotnet48 is broken in Proton prefixes ([#2367](https://github.com/Winetricks/winetricks/issues/2367), [#2246](https://github.com/Winetricks/winetricks/issues/2246)).
  - vcrun2022 failures ([protontricks #321](https://github.com/Matoking/protontricks/issues/321)).
  - Its Qt GUI is weaker than the GTK one (search summary, low weight). We won't use either, since we call the verbs directly.

#### protontricks
- **Status:** GPL-3.0, about 2k stars, 1.14.1 on 2026-03-29 ([repo](https://github.com/Matoking/protontricks)).
- **Use:** only for games run by the native Steam client (`protontricks-launch`, `--gui`). Needs a Steam install.

#### ProtonDB
- **Data:**
  - Summary endpoint returns `{tier, trendingTier, bestReportedTier, confidence, score, total}` ([example](https://www.protondb.com/api/v1/reports/summaries/1245620.json)).
  - Monthly ODbL dumps ([protondb-data](https://github.com/bdefore/protondb-data)).
- **Pitfall:** tiers describe Valve Proton or whatever the reporter used, not our pinned build. Label it "Community rating", separate from our verdict.

#### Steam Deck Verified / SteamOS compatibility
- **Status:** the four categories and criteria are public ([partner doc](https://partner.steamgames.com/doc/steamhardware/compat)). The per-app JSON is undocumented, with new `steamos_`, `machine_` and `frame_` fields ([endpoint](https://store.steampowered.com/saleaction/ajaxgetdeckappcompatibilityreport?nAppID=1245620)).
- **Copy:**
  - The per-check results (input, glyphs, on-screen keyboard, text size, launcher usable with a controller).
  - Try the native build first, then fall back to the Windows build.

#### AreWeAntiCheatYet
- **Status:** MIT, about 500 stars, pushed 2026-09-18. About 1,167 games: Broken 640 / Running 276 / Supported 196 / Denied 53. EAC is the most common anti-cheat ([games.json](https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/master/games.json)).
- **Reuse:** yes, vendor it. Heroic has shown this data on game pages since 2.4.0 (2022) ([#1499](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/1499)).

#### PCGamingWiki
- CC BY-NC-SA 3.0 ([Wikipedia](https://en.wikipedia.org/wiki/PCGamingWiki)).
- The Cargo API needs a bot login since the August 2026 migration, and tables were renamed ([PCGW API](https://www.pcgamingwiki.com/wiki/PCGamingWiki:API), [Playnite ext #110](https://github.com/Jeshibu/PlayniteExtensions/issues/110)).
- **Link out only.**

### D. Sessions, overlays, distros

#### gamescope and gamescope-session
- **Status:** gamescope is BSD-2, about 5k stars, 3.16.30 on 2026-09-24. gamescope-session is MIT, last push 2026-01.
- **Summary:**
  - gamescope needs NVIDIA 515.43.04+ with `nvidia-drm.modeset=1`, or Mesa ([README](https://github.com/ValveSoftware/gamescope)).
  - gamescope-session-plus runs any `CLIENTCMD` as a session ([repo](https://github.com/ChimeraOS/gamescope-session)).
- **Copy:**
  - Presets, with flags visible only in Advanced.
  - A "QindaLutris Big Picture" session built on it.
- **Pitfalls:**
  - NVIDIA HDR flicker and corruption ([NVIDIA forum](https://forums.developer.nvidia.com/t/display-modes-above-2560x1440p-120hz-with-hdr-enabled-cause-flickering-corruption-within-gamescope-session/295314)).
  - Non-Steam games can lose input without `-e` ([#822](https://github.com/ValveSoftware/gamescope/issues/822)).
  - Steam's "Switch to Desktop" needs a `steamos-session-select` script, and users get stuck without one ([guide](https://github.com/Grimish-ng/steam-gamescope-guide/blob/main/README.md)).

#### Decky Loader
- **Status:** GPL-2.0, about 7k stars, v3.2.9 on 2026-09-15 ([repo](https://github.com/SteamDeckHomebrew/decky-loader)).
- **Summary:** a plugin store injected into the Steam UI.
- **Copy:** the ProtonDB Badges plugin's "≥2 agreeing positive reports" rule for suggesting environment tweaks ([protondb-decky](https://github.com/bschelst/protondb-decky)).
- **Avoid:** building anything on Steam UI injection. It breaks with Steam and SteamOS updates ([Steam discussion](https://steamcommunity.com/app/1675200/discussions/0/597396968629646935), [SteamOS #2789](https://github.com/ValveSoftware/SteamOS/issues/2789)).

#### Bazzite
- **Status:** Apache-2.0, about 9k stars, daily builds (testing-44.20260925) ([repo](https://github.com/ublue-os/bazzite)).
- **Summary:**
  - Fedora Atomic with Steam, Lutris and Heroic preinstalled.
  - Deck edition boots into Gaming Mode; NVIDIA images available ([FAQ](https://docs.bazzite.gg/General/FAQ/)).
  - `ujust` recipes: `setup-decky`, `setup-sunshine`, `fix-proton-hang`, `fix-reset-steam`, `setup-boot-windows-steam`, `configure-waydroid` ([ujust](https://docs.bazzite.gg/Installing_and_Managing_Software/ujust/)).
  - Bazzite Portal is a data-driven GUI (yafti) over those recipes, with status checks ([Portal](https://docs.bazzite.gg/Installing_and_Managing_Software/Bazzite_Portal/)).
  - Rollback via rpm-ostree and brh.
- **Copy:**
  - A recipe catalog with status checks.
  - "Fix" buttons named in plain words.
  - A rollback tool usable with a controller.
- **Reuse:** Apache-2.0 snippets can go into GPL-3.0 code, with attribution.
- **Pitfalls:**
  - Layering system packages slows updates, and changes need a reboot ([XDA](https://www.xda-developers.com/most-linux-gamers-only-hear-about-bazzite-but-a-traditional-distro-might-be-the-better-fit/)).
  - Many fixes still start at a terminal.
  - Waydroid doesn't work on NVIDIA ([Waydroid guide](https://docs.bazzite.gg/Installing_and_Managing_Software/Waydroid_Setup_Guide/)).

#### ChimeraOS
- **Status:** MIT, about 2k stars, ChimeraOS 50 [UNSTABLE] on 2026-07-22 ([releases](https://github.com/ChimeraOS/chimeraos/releases)).
- **Summary:**
  - Steam-first, promising "zero configuration for supported games" ([about](https://chimeraos.org/about/)).
  - frzr installs whole-system btrfs images.
  - InputPlumber handles input.
  - A phone web app at `:8844` installs Epic, GOG and Flathub content ([chimera](https://github.com/ChimeraOS/chimera)).
- **Copy:**
  - A "manage from your phone" companion.
  - Automatically adding games to Steam, but *without* the Steam restart it needs.
- **Pitfalls:** slower pace and an older kernel (secondary source: [tech-insider](https://tech-insider.org/bazzite-vs-steamos-vs-chimeraos-2026/)).

#### Nobara
- **Status:** GloriousEggroll's Fedora-based distro. Rolling since 42; release 43 refreshed 2026-04-19 ([Wikipedia](https://en.wikipedia.org/wiki/Nobara_(operating_system)), [linuxcompatible](https://www.linuxcompatible.org/story/nobara-43-20260419-released/)).
- **Copy:**
  - A Welcome app that installs codecs and drivers.
  - A single "Update System" tool (nobara-sync) with post-update quirk fixes and logs ([wiki](https://wiki.nobaraproject.org/general-usage/troubleshooting/updating-troubleshooting)).
  - Controller drivers built in ([controllers](https://wiki.nobaraproject.org/gaming/controllers/xbox)).
- **Pitfalls:** a hobby project with "no professional support" ([nobaraproject.org](https://nobaraproject.org/)).

#### SteamOS
- **Status:** 3.8 installs on AMD desktops as a beta since June 2026; NVIDIA is expected in 2027 ([TheSixthAxis](https://www.thesixthaxis.com/2026/06/22/you-can-now-install-steamos-3-8-on-your-standard-gaming-pc-with-amd-gpu/), [PCWorld](https://www.pcworld.com/article/3172192/valve-is-working-on-nvidia-support-for-steamos-but-dont-expect-it-soon.html)).
  - Intel desktop support is unclear: Wikipedia and the news sources disagree ([Wikipedia](https://en.wikipedia.org/wiki/SteamOS)).
- **Copy:**
  - A/B updates.
  - Verified checks.
  - The per-game "Force the use of a specific compatibility tool" setting ([guide](https://pulsegeek.com/articles/select-compatibility-tools-per-game-in-steam/)).
- **Avoid:** Steam's override that users report can't be unticked ([Steam beta thread](https://steamcommunity.com/groups/SteamClientBeta/discussions/0/4630359277027050301/)). Always make "Use recommended (pinned)" an explicit choice that can be undone.
- **Shaders:**
  - Fossilize pre-caching is for Steam apps only.
  - It can rebuild gigabytes after small updates ([steam-for-linux #13647](https://github.com/ValveSoftware/steam-for-linux/issues/13647)).
  - It can use 20+ GB of RAM until the kernel kills it ([Fossilize #312](https://github.com/ValveSoftware/Fossilize/issues/312)).

#### CachyOS gaming tooling
- **Status:** `cachyos-gaming-meta` and `cachyos-gaming-applications` are installed from CachyOS Hello ([wiki](https://wiki.cachyos.org/configuration/gaming/)).
- **Summary:**
  - The `game-performance` wrapper switches the power profile while a game runs.
  - It recommends `MESA_SHADER_CACHE_MAX_SIZE=12G`.
  - proton-cachyos comes in `-native` and `-slr` builds; the Steam Linux Runtime build is recommended for EAC and BattlEye.
- **Copy:**
  - Automatic performance profile while playing.
  - A larger Mesa cache (Mesa's default is 1 GB, [Mesa env vars](https://docs.mesa3d.org/envvars.html)).
- **Reuse:** proton-cachyos as an optional slot.

#### Goverlay and MangoHud
- **Status:** Goverlay is GPL-3.0, about 1.5k stars, 1.9.3 on 2026-09-25 ([repo](https://github.com/benjamimgois/goverlay)). MangoHud is MIT, about 9k stars, v0.8.4 on 2026-05-27 ([repo](https://github.com/flightlessmango/MangoHud)).
- **Copy:**
  - MangoHud's five presets (off / FPS / horizontal / extended / detailed).
  - Its per-game config lookup order.
  - Goverlay's live preview.
- **Reuse:** both.

### E. Game-specific launchers (anti-cheat and patch handling)

#### Anime Game Launcher family
- **Status:** GPL-3.0, about 2k stars, AAGL 3.19.8 on 2026-09-11, in maintenance mode ([repo](https://github.com/an-anime-team/an-anime-game-launcher)).
- **Summary:**
  - Rust/GTK4.
  - Downloads its own Wine and DXVK from a JSON components index, with a `recommended` flag, `file://` mirrors and a beta channel ([components](https://github.com/an-anime-team/components)).
  - Blocks telemetry.
  - Anti-cheat handling goes through **jadeite**, which warns that using it may breach the game's terms and get users banned ([jadeite](https://codeberg.org/mkrsym1/jadeite)).
- **Copy:**
  - A runner catalog with a "recommended" flag that hides the other versions.
  - Mirror servers.
- **Avoid:** automating patches that breach a game's terms. If we ever surface one, require an explicit ban-risk consent screen.

#### LUG-helper (Star Citizen)
- **Status:** GPL-3.0, about 700 stars, v4.16 on 2026-08-03 ([repo](https://github.com/starcitizen-lug/lug-helper)).
- **Summary:**
  - Zenity GUI that falls back to the terminal.
  - Preflight checks for `vm.max_map_count` and file limits, with fixes via polkit.
  - Runner and DXVK management, reinstall, logs, uninstall.
- **Anti-cheat history:** the `/etc/hosts` EAC workaround was later made obsolete when CIG added a second EAC check, and the wiki now tells users to remove it ([workaround repo](https://github.com/s-iso/SC_EAC_workaround)).
- **Copy:**
  - The preflight check with a one-click fix.
  - The maintenance menu.
- **Avoid:** hosts-file or anti-cheat tricks.

#### Sober and Vinegar (Roblox)
- **Background:** Roblox's Hyperion anti-cheat blocked Wine in 2023, and again on 2024-02-27 after a short return ([Vinegar FAQ](https://vinegarhq.org/Home/rol_faq.html), [It's FOSS](https://itsfoss.com/news/roblox-linux-end/)).
- **Sober:**
  - Runs Roblox's Android x86_64 build natively.
  - Closed source "to reduce the potential for abuse"; Flatpak only ([Sober FAQ](https://vinegarhq.org/Sober/FAQ/index.html), [Flathub](https://flathub.org/en/apps/org.vinegarhq.Sober)).
- **Vinegar** (GPL-3.0, v1.9.4) now covers Roblox Studio only ([repo](https://github.com/vinegarhq/vinegar)).
- **Copy:** have the catalog send known-blocked titles to the working alternative, e.g. "Runs via Sober (Android version)".

### F. Commercial and newcomer references

#### CrossOver
- **Status:** v26 (Feb 2026) with Wine 11 and NTSync ([OMG! Ubuntu](https://www.omgubuntu.co.uk/2026/02/crossover-26-released), [Phoronix](https://www.phoronix.com/news/CrossOver-26)).
- **Install flow for a listed app:**
  1. Search the catalog and click the app's tile.
  2. CrossOver downloads the installer, or asks for the setup file.
  3. It creates a new bottle per app, installs dependencies, and shows the launchers ([support](https://support.codeweavers.com/2-installing-a-listed-application)).
- **CrossTie recipes:**
  - `.tie` recipes are split into application, installation and advanced profiles.
  - Medals: untested / knownnottowork / bronze / silver / gold ([CrossTie](https://support.codeweavers.com/crosstie-data-startpage), [application profile](https://support.codeweavers.com/crosstie-data-startpage/c4-data-application-profile)).
- **Copy:**
  - The 5-level wording.
  - Ratings tied to the version tested ([compat DB](https://support.codeweavers.com/en_US/the-compatibility-database)).
  - Opening a recipe from the browser (a `qindalutris://` URL handler for recipes).
  - A separate guided flow for unlisted apps.
- **Reuse:** none; it is proprietary.

#### Newcomers in 2025–2026
- **Theophany**
  - GPL-3.0; Rust plus **QML**; IGDB metadata; imports from Steam, Heroic and Lutris; Epic via legendary; per-game umu Proton; follows the system accent colour ([repo](https://github.com/oldlamps/theophany)).
  - The closest stack to ours; worth reading as a QML reference.
- **Mira**
  - GPL-3.0; **C++23/Qt6**; a daemon, CLI and GUI talking REST over a Unix socket.
  - Detects games in watched folders, downloads runners, tracks crashes and playtime.
  - A pull request adds Battle.net, Ubisoft, EA and Amazon launcher recipes ([repo](https://github.com/AriGood/Mira), [PR #32](https://github.com/AriGood/Mira/pull/32)).
  - Only 3 stars, but architecturally almost the same as QindaLutris.
- **Zordeer:** GPL-3.0, Qt Widgets, a umu shortcut launcher on Flathub ([repo](https://github.com/Kyuyrii/Zordeer)).
- **zoom-platform.sh:** a store's own installer built on umu and protonfixes, with safety checks and an uninstaller. The store sent its fixes upstream to protonfixes ([GOL](https://www.gamingonlinux.com/2024/10/zoom-platform-store-announces-new-tool-to-run-windows-games-on-linux-with-proton/)).
- **WinBoat**
  - MIT; runs real Windows in a container with KVM and shows app windows via RemoteApp.
  - **No GPU acceleration.** Its docs say "kernel anti-cheat… not possible, as they block virtualization" ([winboat.app](https://www.winboat.app/), [gHacks](https://www.ghacks.net/2025/12/02/run-any-windows-app-on-linux-with-winboat-its-free-and-open-source/)).
  - A possible fallback for *non-game* Microsoft Store apps, not for games.
- **armysarge/Proton-Game-Launcher:** 0 stars, no license, reads OnlineFix.ini (which suggests pirated copies). Do not associate with it. Its only interesting idea is a one-click "Install C++ Runtime" ([repo](https://github.com/armysarge/Proton-Game-Launcher)).
- **Many tiny umu front ends** under the GitHub topic ([topic](https://github.com/topics/umu-launcher)). umu is now the de facto backend.

---

## 4. Known hard limits, and how to say so honestly in the UI

| Limit | Why | Suggested UI (before install) |
|---|---|---|
| **Kernel-level anti-cheat** (Riot Vanguard, CoD Ricochet, FACEIT, EA Javelin in Battlefield 6, …) | These need a Windows kernel driver; there is no Linux path, and Javelin launched with BF6 on 2025-10-10 ([caniplayonlinux](https://caniplayonlinux.com/guides/anti-cheat-linux-2026/), [It's FOSS](https://itsfoss.com/news/ea-anti-cheat-expansion-plans/)). Running Windows in a VM also fails, because they block virtualization ([WinBoat](https://www.winboat.app/)). | A red "**Can't run on Linux**" card. Say it's the publisher's anti-cheat choice, not a missing setting. Disable Install. Offer cloud streaming only if the game is available there. |
| **Anti-cheat "Denied" or "Broken"** (EAC/BattlEye not enabled by the developer) | Proton EAC/BattlEye support is **opt-in by the developer** ([Steamworks Proton doc](https://partner.steamgames.com/doc/steamdeck/proton), [BattlEye opt-in](https://store.steampowered.com/news/group/4145017/view/3104663180636096966)). AreWeAntiCheatYet lists 53 Denied and 640 Broken ([games.json](https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/master/games.json)). | "**Online play blocked by the developer**", with a link to the AreWeAntiCheatYet source. If the single-player mode works, say so. Never offer a hosts-file or loader workaround (LUG-helper history, jadeite ban warning). |
| **Microsoft Store / Game Pass PC** (UWP, MSIX, MSIXVC) | Wine has no MSIX/UWP support ([WineHQ forum](https://forum.winehq.org/viewtopic.php?t=36418)). **Xodus** can sign in, download and license, but games **don't run yet**. It is not endorsed by Microsoft, and MSIXVC2 is not supported ([GOL](https://www.gamingonlinux.com/2026/08/xbox-pc-and-game-pass-coming-to-linux-with-the-xodus-project/), [XDA](https://www.xda-developers.com/xbox-game-pass-games-might-soon-work-on-linux-thanks-to-this-open-source-project/)). | "**Not available for Microsoft Store games.** Many are also sold on Steam, Epic or GOG; check there." Offer **cloud play** as a fallback: Game Pass Ultimate via xCloud using Greenlight ([Greenlight](https://github.com/unknownskl/greenlight)) or the browser, or **GeForce NOW**, whose native Linux app has been in beta since 2026-01 ([GOL](https://www.gamingonlinux.com/2026/01/the-native-linux-app-for-nvidia-geforce-now-is-now-in-beta/)). Keep an eye on Xodus. |
| **Wine-blocked titles** (Roblox Player) | Hyperion blocks Wine ([Vinegar FAQ](https://vinegarhq.org/Home/rol_faq.html)). | Send users to **Sober** (Flathub) with a note: "unofficial; Roblox could block it at any time" ([Sober FAQ](https://vinegarhq.org/Sober/FAQ/index.html)). |
| **Denuvo activation limits** | Recreating a prefix can use up an activation, and users may wait about 24 h ([Heroic wiki](https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/wiki/Troubleshooting-Heroic-and-Games)). | Detect Denuvo (Eagle-style). Warn before "Reset" and prefer restoring a snapshot to a fresh prefix. |
| **Store launchers that break on their own** (Battle.net, EA app updates) | Vendor updates change behaviour outside our control ([Bottles #3721](https://github.com/bottlesdevs/Bottles/issues/3721), [Faugus #538](https://github.com/Faugus/faugus-launcher/issues/538)). | Show a "Launcher needs repair" status with **Repair** (rerun the recipe steps against a snapshot). Be upfront that official launchers sometimes update in ways that break. |
| **Offline first launch** | umu and Proton may need to download components ([umu #331](https://github.com/Open-Wine-Components/umu-launcher/issues/331)). | Pre-install via Portage. If something is still missing, say "Needs internet once to finish setting up this game". |
| **Shader stutter on first play (non-Steam games)** | Fossilize pre-caching is Steam-only, and the DXVK state cache is gone ([DXVK 2.7](https://github.com/doitsujin/dxvk/releases/tag/v2.7)). | Keep a persistent per-game shader cache and a larger Mesa cache. Tell users: "The first few minutes may stutter while graphics are prepared; this goes away." |
| **Steam Input outside Steam** | Steam Input only exists inside Steam ([gamescope #438](https://github.com/ValveSoftware/gamescope/issues/438)). | Use InputPlumber and SDL mappings. For games that need Steam Input, offer "Add to Steam". |
| **NVIDIA with gamescope HDR** | Corruption and flicker in the driver ([NVIDIA forum](https://forums.developer.nvidia.com/t/display-modes-above-2560x1440p-120hz-with-hdr-enabled-cause-flickering-corruption-within-gamescope-session/295314)). | Keep gamescope off and HDR hidden on NVIDIA by default, with an explanation in the tooltip. |
| **Games that need DRM-breaching or ToS-breaching patches** | Ban risk ([jadeite](https://codeberg.org/mkrsym1/jadeite)). | Don't automate. At most, link to the project with a ban-risk statement. |

**Principle.** Every verdict should name its source (our testing, AreWeAntiCheatYet, Valve, ProtonDB) and the date. That way "can't run" reads as a fact about the game and not a failure of the system.

---

## 5. Sources

Repo metadata (stars, license, last push) came from `api.github.com/repos/<owner>/<repo>`, and latest releases from `github.com/<owner>/<repo>/releases.atom`, both on 2026-09-25. All other URLs are cited inline above.

**Launchers and helpers**
- https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher
- https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/releases/tag/v2.22.0
- https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/5893
- https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/5921
- https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/4901
- https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/1499
- https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/issues/1513
- https://github.com/Heroic-Games-Launcher/HeroicGamesLauncher/wiki/Troubleshooting-Heroic-and-Games
- https://github.com/Heroic-Games-Launcher/known-fixes
- https://github.com/Heroic-Games-Launcher/heroic-gogdl
- https://github.com/derrod/legendary
- https://github.com/imLinguin/nile
- https://www.gamingonlinux.com/2026/09/heroic-games-launcher-v2-22-2-brings-fixes-for-rockstar-ubisoft-and-more/
- https://www.linuxcompatible.org/story/heroic-games-launcher-2223-hotfix-repairs-broken-windowsonlinux-game-launches
- https://steamdeckhq.com/news/heroic-launcher-2-16-0-update/
- https://shattered.io/heroic-vs-lutris/

**Lutris**
- https://github.com/lutris/lutris
- https://github.com/lutris/lutris/blob/master/docs/installers.rst
- https://github.com/lutris/website/wiki/API-Documentation
- https://lutris.net/api/installers/battlenet
- https://lutris.net/api/runtimes
- https://github.com/lutris/docs/blob/master/Battle.Net.md
- https://github.com/lutris/lutris/issues/3117
- https://github.com/lutris/lutris/issues/5998
- https://github.com/lutris/lutris/issues/5506
- https://github.com/lutris/lutris/issues/5914
- https://github.com/lutris/lutris/issues/6402
- https://github.com/lutris/lutris/issues/6406
- https://github.com/lutris/lutris/issues/6665
- https://www.gamingonlinux.com/2026/02/game-manager-lutris-v0-5-20-released-with-proton-upgrades-store-updates-and-much-more/
- https://www.gamingonlinux.com/2026/02/lutris-v0-5-21-and-v0-5-22-arrive-with-valves-sniper-runtime-support-and-new-game-runners/

**Other launchers and library front ends**
- https://github.com/RareDevs/Rare
- https://github.com/sharkwouter/minigalaxy
- https://github.com/kra-mo/cartridges
- https://playnite.link/
- https://www.gamingonlinux.com/2025/11/playnite-may-get-a-linux-version-during-2026-as-the-creator-plans-a-move-to-linux/
- https://www.gamingonlinux.com/2026/08/game-launcher-launchbox-should-get-a-linux-version-in-2027/

**Bottles**
- https://docs.usebottles.com/bottles/versioning.md
- https://docs.usebottles.com/bottles/installers.md
- https://docs.usebottles.com/bottles/dependencies.md
- https://docs.usebottles.com/getting-started/environments.md
- https://github.com/bottlesdevs/Bottles/wiki/Packaging
- https://github.com/bottlesdevs/Bottles/issues/3721
- https://www.gamingonlinux.com/2026/08/bottles-game-manager-v66-brings-umu-from-open-wine-components-to-help-run-games/
- https://www.abit.ee/en/soft/operating-systems/bottles-610-wine-linux-run-windows-programs-eagle-tool-executable-analysis-proton-compatibility-gami-en

**Faugus, PortProton, WineZGUI**
- https://github.com/Faugus/faugus-launcher
- https://github.com/Faugus/faugus-launcher/issues/538
- https://www.gamingonlinux.com/2026/07/faugus-launcher-2-0-rolls-out-with-a-new-ui-and-many-other-enhancements/
- https://github.com/Castro-Fidel/PortWINE
- https://raw.githubusercontent.com/linux-gaming-ru/PortProtonQt/main/README.md
- https://github.com/flathub/flathub/issues/5195
- https://github.com/fastrizwaan/WineZGUI
- https://github.com/fastrizwaan/WineCharm

**Proton managers and Steam Tinker Launch**
- https://github.com/DavidoTek/ProtonUp-Qt
- https://github.com/Vysp3r/ProtonPlus
- https://www.gamingonlinux.com/2026/08/protonplus-v0-6-0-adds-support-for-faugus-launcher-improved-controller-navigation-and-more/
- https://github.com/sonic2kk/steamtinkerlaunch

**umu stack, winetricks, protontricks**
- https://raw.githubusercontent.com/Open-Wine-Components/umu-launcher/main/README.md
- https://raw.githubusercontent.com/Open-Wine-Components/umu-launcher/main/docs/umu.1.scd
- https://man.archlinux.org/man/umu.1.en
- https://github.com/Open-Wine-Components/umu-launcher/issues/331
- https://github.com/Open-Wine-Components/umu-launcher/issues/603
- https://raw.githubusercontent.com/Open-Wine-Components/umu-protonfixes/master/README.md
- https://raw.githubusercontent.com/Open-Wine-Components/umu-database/main/umu-database.csv
- https://raw.githubusercontent.com/Winetricks/winetricks/master/README.md
- https://github.com/Winetricks/winetricks/issues/2367
- https://github.com/Matoking/protontricks

**Compatibility data**
- https://www.protondb.com/api/v1/reports/summaries/1245620.json
- https://github.com/bdefore/protondb-data
- https://store.steampowered.com/saleaction/ajaxgetdeckappcompatibilityreport?nAppID=1245620
- https://partner.steamgames.com/doc/steamhardware/compat
- https://www.steamdeck.com/en/verified
- https://raw.githubusercontent.com/AreWeAntiCheatYet/AreWeAntiCheatYet/master/games.json
- https://www.pcgamingwiki.com/wiki/PCGamingWiki:API
- https://github.com/arelate/vangogh/issues/212
- https://en.wikipedia.org/wiki/PCGamingWiki

**Anti-cheat and Microsoft Store / Game Pass**
- https://partner.steamgames.com/doc/steamdeck/proton
- https://caniplayonlinux.com/guides/anti-cheat-linux-2026/
- https://itsfoss.com/news/ea-anti-cheat-expansion-plans/
- https://www.gamingonlinux.com/2026/08/xbox-pc-and-game-pass-coming-to-linux-with-the-xodus-project/
- https://www.xda-developers.com/xbox-game-pass-games-might-soon-work-on-linux-thanks-to-this-open-source-project/
- https://github.com/unknownskl/greenlight
- https://www.gamingonlinux.com/2026/01/the-native-linux-app-for-nvidia-geforce-now-is-now-in-beta/

**Shaders**
- https://github.com/doitsujin/dxvk/releases/tag/v2.7
- https://github.com/ValveSoftware/Fossilize
- https://github.com/ValveSoftware/Fossilize/issues/312
- https://github.com/ValveSoftware/steam-for-linux/issues/13647
- https://docs.mesa3d.org/envvars.html

**gamescope and Decky**
- https://github.com/ValveSoftware/gamescope
- https://github.com/ChimeraOS/gamescope-session
- https://github.com/SteamDeckHomebrew/decky-loader
- https://github.com/bschelst/protondb-decky

**Distros and sessions**
- https://docs.bazzite.gg/Installing_and_Managing_Software/ujust/
- https://docs.bazzite.gg/Installing_and_Managing_Software/Bazzite_Portal/
- https://docs.bazzite.gg/Installing_and_Managing_Software/Updates_Rollbacks_and_Rebasing/bazzite_rollback_helper/
- https://chimeraos.org/about/
- https://github.com/ChimeraOS/chimera
- https://wiki.nobaraproject.org/gaming/controllers/xbox
- https://wiki.nobaraproject.org/general-usage/troubleshooting/updating-troubleshooting
- https://www.thesixthaxis.com/2026/06/22/you-can-now-install-steamos-3-8-on-your-standard-gaming-pc-with-amd-gpu/
- https://wiki.cachyos.org/configuration/gaming/
- https://github.com/CachyOS/proton-cachyos

**Controllers**
- https://github.com/ShadowBlip/InputPlumber
- https://www.xda-developers.com/bazzite-reveals-the-open-gaming-collective-to-make-gaming-on-linux-even-better/
- https://github.com/mdqinc/SDL_GameControllerDB
- https://raw.githubusercontent.com/GloriousEggroll/proton-ge-custom/master/docs/CONTROLLERS.md

**Overlays**
- https://github.com/flightlessmango/MangoHud
- https://github.com/benjamimgois/goverlay

**Game-specific launchers**
- https://github.com/an-anime-team/an-anime-game-launcher
- https://github.com/an-anime-team/components
- https://codeberg.org/mkrsym1/jadeite
- https://github.com/starcitizen-lug/lug-helper
- https://github.com/s-iso/SC_EAC_workaround
- https://vinegarhq.org/Sober/FAQ/index.html
- https://vinegarhq.org/Home/rol_faq.html
- https://github.com/vinegarhq/vinegar

**CrossOver**
- https://www.codeweavers.com/compatibility/rating-system
- https://support.codeweavers.com/2-installing-a-listed-application
- https://support.codeweavers.com/crosstie-data-startpage

**Newcomers**
- https://github.com/oldlamps/theophany
- https://github.com/AriGood/Mira
- https://github.com/Kyuyrii/Zordeer
- https://www.winboat.app/
- https://github.com/armysarge/Proton-Game-Launcher
- https://github.com/topics/umu-launcher
