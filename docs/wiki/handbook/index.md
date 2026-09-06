# QindaQt handbook

QindaQt is a desktop built on Qt and Wayland — the screen you see after
logging in, the top panel and the dock, the apps that come with it, and
the way windows group together. It is pronounced **“kinda cute.”**

These guides explain what you can do with QindaQt and what happens when you
do it. They describe the software as it actually behaves; anything still
being rolled out or verified is called out where it applies.

## Your first ten minutes

1. **Look around.** A slim **top panel** runs across the top of the screen
   and a **dock** sits at the bottom. The top panel's left button opens the
   system menu (settings, lock, log out, restart, shut down); next to it, the
   menu of the app you're working in appears automatically.
2. **Open an app.** Click the launcher in the dock, type a name or browse
   the categories, and open something. Running apps appear in the dock as
   well.
3. **Try the signature move.** Hold **Meta+Shift** and drag one window onto
   another: drop it in the middle to stack the two as tabs, or near an edge
   to place them side by side. One frame now holds both, and the dock shows
   them as a single entry. Drag a window's own title strip back out to make
   it independent again.
4. **Change how it looks.** Open **System Settings → Appearance** and try a
   theme, a wallpaper, or a different font. The page previews every change;
   **Apply** saves it.
5. **Lost?** Press **Meta+F1**. A small card on the desktop lists the default
   shortcuts for combining and arranging windows, and the **Got it** button
   puts it away until you need it again.

From here, [Using the desktop](desktop.md) covers windows, menus, and
notifications; [Making it yours](customization.md) covers themes, wallpapers,
fonts, and layouts; [Applications](applications.md) covers the programs that
ship with QindaQt.

## Choose a reading path

| Your question | Start here |
| --- | --- |
| How do I use the desktop day to day? | [Using the desktop](desktop.md) |
| How do I change the look and layout? | [Making it yours](customization.md) |
| Which applications are included, and what do they do? | [Applications](applications.md) |
| What does a term mean? | [Glossary](glossary.md) |
| Why does this project exist? | [Project and philosophy](project.md) |
| How does it work internally? | [Architecture](architecture.md) |
| What services and integrations exist? | [Platform services](platform.md) |
| What is saved, protected, or recovered after failure? | [Privacy and persistence](privacy.md) |
| How do I build, run, and diagnose it? | [Development and operation](development.md) |
| How are quality and progress established? | [Quality and contribution](quality.md) |
| What is every tracked feature's exact status? | [Feature catalog](catalog/features.md) |
| What are all the preferences? | [Settings catalog](catalog/settings.md) |
| Which profiles, themes, and applets ship? | [Packaged assets](catalog/assets.md) |
| Where is the code and supporting tooling? | [Repository catalog](catalog/repository.md) |
| Where are all detailed specifications and decisions? | [Documentation catalog](catalog/reading.md) |

The first four pages are written for someone using the desktop. The rest are
reference and contributor material, kept separate so neither bloats the
other.

## About these guides

The user-facing pages follow a few rules, kept here so future pages stay
consistent:

- Write to a person. Say **you** and the name of the thing on screen, explain
  what happens next, and prefer one concrete instruction over a paragraph of
  nouns.
- Only describe controls and shortcuts that really exist, with the labels
  they actually carry. Never invent a button, gesture, or key.
- Say what a feature does *for* the person, and be honest about anything not
  finished yet — an unfinished feature is named as such, not advertised.
- Keep engineering detail out of the flow. When a reader needs the full
  contract, link to the page that owns it instead of summarizing it inline.

Guides describe the current build. Exact, per-feature maturity — including
what is integrated, what is deployed, and what is still being verified — is
tracked in the [feature catalog](catalog/features.md); the delivery boundary
is recorded in `docs/HANDOFF.md`. A guide and reality disagreeing
is a defect in the guide.

When behavior changes, update the page that owns the behavior and the
relevant guide here in the same change, and register any new page in
`mkdocs.yml`. Adding a page does not by itself make a feature real.
