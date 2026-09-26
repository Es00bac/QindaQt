# QindaQt showcase website

`website/` is a portable static site. Serve it from the repository root with:

```sh
python3 -m http.server 8765 --directory website
```

Open `http://localhost:8765/`. QindaOffice has its own nested page at
`/office/`; later companion projects can add sibling directories while sharing
`assets/base.css`, `assets/identity.css`, and the same header/footer pattern. There is no site build step,
framework runtime, tracking script, or remote font request. The source links
point to GitHub; installation is intentionally directed to each repository's
current instructions rather than to an unqualified binary download.
Run `python3 website/scripts/check-site.py` to validate local links, media,
captions, and basic image/button markup.

## Evidence and media

- `desktop-*.webp` and `office-*.webp` are real captures from an isolated
  1440 × 900 virtual QindaQt session, made by `scripts/capture-live.sh` or
  selected from the live recording frames produced by `scripts/record-live.sh`.
  Spectacle captures the frames and FFmpeg converts them to WebP. Both scripts use a private D-Bus
  session and XDG configuration, data, cache, and Desktop directories under
  ignored `build/site-capture/`; it disables normal autostart. It never copies
  a capture of the owner's live desktop or personal files into the site.
- `profile-*.webp` are deterministic renders from QindaQt's
  `qindaqt-shell-preview` against this checkout's profile/theme catalogs.
  The site labels them “Shell render · preview” so they are not mistaken for
  live session screenshots. Source command pattern:

  ```sh
  QT_QPA_PLATFORM=offscreen qindaqt-shell-preview \
    --profile-dir data/profiles --theme-dir data/themes \
    --profile qindaqt --theme qinda-dark \
    --width 1440 --height 900 --screenshot build/site-capture/profile-signature.png
  ```

- The six wallpaper WebPs are format-optimized copies of
  `data/wallpapers/*.png`; their original source and provenance are in
  `data/wallpapers/ARTWORK.md`.
- `cyber-pengu-ducke.png` was made with the built-in image generation tool using
  `data/wallpapers/compile-club.png` as the identity reference. Final prompt:
  “Extract its two characters, Cyber Pengu and Ducke, together as a clean
  full-body illustrated duo on genuinely transparent background. Preserve their
  exact character designs, expression and painterly anime rendering: Cyber
  Pengu is a round black-and-white chibi penguin with one luminous jade cyber
  visor eye, little yellow beak, graphite armor, jade Q patch, oversized amber
  boots, coffee mug and holographic device; Ducke is the little yellow rubber
  duck in black sunglasses and mechanical armor with the fan. Remove all
  workshop, server, floor, shadows beyond the characters, text, and props not
  carried by them. Keep clean edges. Do not redesign or add text.”
- The illustrated window arrangement on the home page is HTML/CSS, not a
  screenshot. It follows the handbook's documented tab/split/detach behavior.

## Working Office project

The Office subsite follows **Signal Garden**, a fictional neighborhood light
and sound event. Its downloadable `assets/sample/signal-garden-project.zip`
contains a complete editable brief (`.docx`), two budget/scenario workbooks
(`.xlsx`, with formulas and a chart), a styled native QindaShow deck (`.fodp`,
including speaker notes), and a two-page native QindaNote notebook. The
figures and approval decision agree across the files. `office-*.webp` show
those actual files opened by the installed apps; clicking a screenshot opens
the full capture. The ZIP contains only the public sample files.

Rebuild the project with `python-docx` and `openpyxl` installed:

```sh
python website/scripts/build-office-sample.py
```

The FODP is generated directly to the suite's documented native shape/text
format so the presentation's color, type, and speaker notes survive reopening.
`scripts/capture-live.sh` copies the sample into a temporary public path
before opening it, keeping personal paths out of captures. It creates fresh
XDG state for each run so an earlier app autosave cannot cover a screenshot
with a recovery dialog. The isolated session may report unavailable desktop
portals in its log; successful capture and visual inspection of all eight
live screenshots are the acceptance evidence.

## Narrated films

`scripts/record-live.sh` drives the installed desktop and Office apps inside
a scenario-gated virtual QindaQt session. It uses the compositor's private
development input interface to click, drag, and operate public sample files;
it never sends input to the owner's desktop. It records frame sequences with
Spectacle under ignored `build/site-capture/live/`. The desktop sequence
includes an actual Meta+Shift drag that creates a split, a layout switch in
Settings, and native File Manager. The Office sequence edits the DOCX brief,
changes a cell in the XLSX budget so formulas recalculate, opens the workbook
chart, advances the native slides with speaker notes, and changes notebook
pages. The sample files are copied to `/tmp` before the recording and removed
afterward.

`scripts/render-live-videos.py` is the storyboard, narration, caption generator,
and reproducible film edit. It overlays moving cutouts of Cyber Pengu and
Ducke from the site's wallpaper-referenced mascot illustration. They respond
to the work in every chapter; brief focus rings and speech cues point to real
controls and outcomes. The app UI under them is live footage. WebVTT captions
ship beside the two MP4s.

The character narration uses two contrasting [Kokoro](https://huggingface.co/hexgrad/Kokoro-82M)
voices (`bm_george` for Cyber Pengu and `am_puck` for Ducke), voiced with
[kokoro-onnx](https://github.com/thewh1teagle/kokoro-onnx). The scene scripts
use the project's pronunciation (“Kinda Cute”) in speech while captions keep
the QindaQt spelling. This does not use
the desktop's ElevenLabs speech helper. Kokoro model/voices are Apache-2.0;
kokoro-onnx is MIT. Model weights are build inputs and are not committed.
To rebuild, install `kokoro-onnx`, `soundfile`, and `Pillow` into a Python environment,
download `kokoro-v1.0.int8.onnx` and `voices-v1.0.bin` from the
`model-files-v1.1` release of kokoro-onnx, then run:

```sh
website/scripts/record-live.sh
python website/scripts/render-live-videos.py \
  --model build/site-capture/tts-model/kokoro-v1.0.int8.onnx \
  --voices build/site-capture/tts-model/voices-v1.0.bin
```

Pass `QINDA_SCENE=office-calc` to the recording script to update one scene,
and `--film office-tour` to the renderer to rebuild one film.

## Content rules

The [QindaQt handbook](../docs/wiki/handbook/index.md) and the separate
QindaOffice `README.md`/`docs/index.md` are source truth. When a promoted
interaction changes, update this site and its captions alongside the owning
wiki page. Keep status honest: QindaQt is free and open source, and under
active development; package availability is not the same as a fully qualified
release. For a new sub-project, add a nested directory and project-specific
evidence instead of implying that a separate repository is built into the
desktop package.
