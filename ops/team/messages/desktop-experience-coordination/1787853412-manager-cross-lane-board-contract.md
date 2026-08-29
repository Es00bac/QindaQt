# Manager decision: native apps, shell, settings, and platform services coordinate on-board

- **Timestamp:** 2026-08-27T11:56:52-06:00
- **From:** Manager
- **Applies to:** native-application design, shell/customization, Settings1,
  platform services, themes/profiles, and their reviewers

Parallel lanes must use this shared board as an interface-design channel, not
only as a final handoff archive. A worker that discovers a question, proposes a
consumer/provider contract, changes direction, finds a collision, or can answer
another lane must post a new timestamped Markdown record. Do not leave that
knowledge only in a provider transcript, ephemeral chat, private build output,
or manager summary.

## Cross-lane question format

Every question record names:

- `From` and `To` lane/worker;
- the user-visible decision it affects;
- the exact public interface or interaction in question;
- the proposed default and alternatives;
- owned and potentially colliding paths;
- whether work can continue safely before the answer;
- the evidence or decision requested.

The answering worker posts a new reply rather than editing the question. Both
the question and answer link to the owning architecture/design handoff and, if
accepted, the owning wiki page or ADR update. A manager decision that changes a
public boundary is posted separately so every lane can cite one canonical
resolution.

## Current routing

- Native-app/design-system records:
  `ops/team/messages/native-application-design/`
- Platform-service records:
  `ops/team/messages/platform-services/`
- Current Settings1 outcome:
  `ops/team/messages/persistent-notification-quieting/`
- Shell live-notification qualification receives its own outcome thread after
  the accepted Settings1 commit integrates.

Until that shell thread exists, app-designer questions for shell/customization
are posted under native-application design and linked here. The manager will
route them into the shell assignment; the future shell owner must read and
answer the complete linked thread before changing the corresponding boundary.

## Boundary rule

Questions and design feedback do not authorize one lane to edit another lane's
owned paths. Cross-lane changes still go through the public boundary, explicit
path coordination, exact candidate review, and same-change wiki/ADR update
required by `AGENTS.md`.
