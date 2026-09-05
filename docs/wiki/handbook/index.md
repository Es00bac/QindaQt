# QindaQt handbook

This is the organized entry point for understanding QindaQt: its purpose, design
philosophy, desktop features, applications, internals, configuration, development,
and delivery status. QindaQt is pronounced **“kinda cute.”**

## Choose a reading path

| Your question | Start here |
| --- | --- |
| What is this project and why does it exist? | [Project and philosophy](project.md) |
| What does the desktop let me do? | [Desktop experience](desktop.md) |
| What applications are included? | [Applications](applications.md) |
| How do themes, layouts, and preferences fit together? | [Customization](customization.md) |
| How does it work internally? | [Architecture](architecture.md) |
| What services and integrations exist? | [Platform services](platform.md) |
| What is saved, protected, or recovered after failure? | [Privacy and persistence](privacy.md) |
| How do I build, run, and diagnose it? | [Development and operation](development.md) |
| How are quality and progress established? | [Quality and contribution](quality.md) |
| What does the terminology mean? | [Glossary](glossary.md) |
| What is every tracked feature's exact status? | [Feature catalog](catalog/features.md) |
| What are all the preferences? | [Settings catalog](catalog/settings.md) |
| Which profiles, themes, and applets ship? | [Packaged assets](catalog/assets.md) |
| Where is the code and supporting tooling? | [Repository catalog](catalog/repository.md) |
| Where are all detailed specifications and decisions? | [Documentation catalog](catalog/reading.md) |

## Scope and source of truth

This handbook was assembled against repository commit
`9728612046940b55d69f85c3811eb38a08a0963b` on 2026-09-05. Catalogs describe
that snapshot; they are not fresh hardware or runtime qualification results.
The feature catalog enumerates the complete outcome ledger, including unfinished
work. The documentation catalog indexes all existing canonical wiki pages and
ADRs; the configuration catalog enumerates shipped data rather than guessing
support from a UI mockup. The repository catalog provides the implementation
and test entry points for deeper inspection.

The existing [project wiki](../index.md) remains the normative contract source.
This directory adds explanations and a categorized reference layer, without
forking protocol definitions or copying every source file into prose. Internal
symbols and individual historical worker messages remain in their owning source
and delivery records; the handbook explains where and how to inspect them.

Some older overview paragraphs describe earlier slices. For present delivery
status, consult the exact stopping points in `ops/team/features.json`, then the
latest `docs/HANDOFF.md` and `docs/TASK_LIST.md` entries and focused module pages.
Historical qualification against KWin 6.6.5 is not the current build pin: the
snapshot's `compositor/upstream/kwin.json` pins **6.6.6**. A disagreement should
be resolved with integrated evidence, not by silently promoting a feature.

## Keeping this library complete

When behavior changes, update the canonical owner page and the relevant guide
or catalog here together. Add new features to the ledger before describing
maturity; retain caveats and evidence references. Reconcile package inventories
with `data/`, module inventories with `src/`, and document inventories with the
wiki. Register new pages in `mkdocs.yml`, keep links reciprocal, and run the
[documentation checks](quality.md). No roadmap milestone is advanced by adding
this handbook.
