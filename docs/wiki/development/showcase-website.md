# Showcase website

The static `website/` tree introduces QindaQt to potential users. It explains
the documented window-container gesture, profiles, appearance, first-party
applications, and the separately packaged QindaOffice suite. The QindaOffice
page lives at `/office/`; future companion projects can use sibling paths.
Shared styles and navigation remain under `website/assets/`. The presentation
uses a graphite, sea-glass, and apricot workspace design with a fixed rail,
compact annotations, and large live captures. The eleven shell profiles are
starting points; the site shows their paired appearance themes and wallpapers.
Audio1 has a separate deep dive at `/audio/` with a live console capture,
captioned film, and an interactive explanatory signal-path diagram.

The site's behavior claims follow the [handbook](../handbook/index.md),
especially [Using the desktop](../handbook/desktop.md),
[Making it yours](../handbook/customization.md), and
[Applications](../handbook/applications.md). QindaOffice claims follow that
repository's own current documentation. The site links to source and the
handbook instead of implying a stable one-click binary release while the
[release qualification](implementation-roadmap.md) remains in progress.

The [website README](https://github.com/Es00bac/QindaQt/blob/main/website/README.md)
records the local server,
capture method, media provenance, mascot prompt, and film generation. Live
screenshots come from an isolated virtual QindaQt session with an empty Desktop
directory; five profile comparisons are live captures with paired themes,
while the remaining previews are explicitly labeled renders from
`qindaqt-shell-preview`. The HTML window demo is an illustration of the real
tab/split/detach contract. Three captioned films use actual screen recordings
from the private virtual session: the desktop film shows a real Meta+Shift
split and a live layout change; the Office film operates the sample files in
installed apps; the Audio1 film walks actual Settings controls. Cyber Pengu
and Ducke switch wallpaper-matched speaking and gesture poses in each chapter,
voiced with distinct Kokoro voices. No personal desktop capture or model
weights are in the website tree.

The Office page is an inspectable case study of a fictional community event,
Signal Garden. It ships a real DOCX brief, two XLSX budget/scenario workbooks
with formulas and a chart, a native FODP pitch deck with speaker notes, and a
two-page QindaNote notebook. The downloadable ZIP holds the same documents
shown in the QindaOffice screenshots and narrated tour. `website/scripts/build-office-sample.py`
rebuilds the documents; the capture script opens them in the installed suite
inside the private virtual session. The FODP uses QindaShow's native format
because a converted PPTX did not preserve the slide presentation faithfully
in the installed build.

When changing promoted behavior or media, update the owning wiki page and the
site together. Verify the static pages at desktop and mobile widths, local
links/media paths, interactions, and captions. `mkdocs build --strict` and
the repository link checker remain the documentation gates.
