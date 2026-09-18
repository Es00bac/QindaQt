# ADR-0201: Quiet hours are a window the settings service owns

- **Status:** Proposed
- **Date:** 2026-09-18
- **Owners:** Notifications, Settings
- **Supersedes:** None
- **Superseded by:** None

## Context

Do Not Disturb already exists and is one boolean: `services.doNotDisturb`.
`NotificationInterruptionPolicy` reads it and admits only critical popups
while it is set, the shell's `NotificationQuietingSettingsBridge` carries it
from Settings1 into the policy, and the Notifications route in Settings
offers the switch. What has been missing is the part users actually ask for:
*quiet at night, loud in the morning*, without remembering to flip the switch
twice a day.

Two obvious shapes were rejected before this one.

A **timer in the shell** — arm a `QTimer` for 22:00, flip `doNotDisturb`,
arm another for 07:00, flip it back — makes the schedule a side effect on a
user-owned key. A session that starts at 23:00 has missed its own edge, a
session that is asleep at 07:00 wakes up quiet, and the switch on the
Notifications page would move on its own, so a user could no longer tell
their own choice from the schedule's. Worse, the shell would be writing a
key the user also writes, and the last writer would win.

A **cron- or systemd-timer-driven** version has the same problem one process
further away, and adds a unit to install and a clock to keep in step.

## Decision

**A quiet-hours schedule is three additive Settings1 keys and a pure
predicate over the clock. Nothing writes `services.doNotDisturb` on the
user's behalf, and nothing schedules anything.**

- The keys are `services.doNotDisturbSchedule` (boolean),
  `services.doNotDisturbStartMinutes` and `services.doNotDisturbEndMinutes`
  (integers, 0..1439). They extend `data/settings/schema-v2.json` additively
  and the schema stays at version 2, the precedent ADR-0028 records.
- `NotificationInterruptionPolicy` gains a `QuietHours` window and an
  injected `MinuteOfDayClock`. `allowsPopup` admits everything when neither
  Do Not Disturb nor the window is in force, and otherwise admits only
  critical urgency — exactly the rule Do Not Disturb already had. The two
  reasons to be quiet are a disjunction, never a write.
- A window whose start is greater than its end wraps midnight. A window
  whose ends are **equal is empty**, not all day: a user who has not chosen
  two times has not asked for silence.
- The Notifications route publishes only what the settings service last
  confirmed. A refused or silently-ignored edit leaves every control where
  it was, and the reason is shown beside it.

## Consequences

- The user's own Do Not Disturb switch keeps meaning what they set. The
  schedule is a second, independent reason for quiet, and the summary line on
  the page says in words which window is in force.
- The policy stays a pure function of (settings, clock, notification). It is
  tested without a bus, without a timer, and without a real clock, including
  both sides of a wrapping window and the empty-window case.
- Nothing runs between events: the window is evaluated when a notification
  arrives. There is no drift to correct, no missed edge after a resume, and
  nothing to unschedule at logout.
- The three keys have a consumer from the day they exist, so they cannot
  become dead knobs on the settings page.
- The shell's notification-center quick control still shows only the Do Not
  Disturb switch. While a quiet window is in force and the switch is off,
  that control reads "off" while low and normal popups are being held, and
  only the Settings route says why. Surfacing the active window there is
  follow-up work, not a change to this decision.
- A schedule per weekday, or more than one window a day, is not expressible.
  Adding either means new keys and a re-reading of this decision, not a
  reinterpretation of these three.

## Revisit when

A user needs different windows on different days, or needs the schedule to
take effect on something that is not a notification popup — a wallpaper
change, an input-method switch. Either would make the window a session-wide
mode rather than one input to one policy, and a mode does want an owner that
runs.
