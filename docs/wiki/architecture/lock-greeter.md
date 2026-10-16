# Lock greeter lifecycle

`src/lock_greeter` is the protected locker's prompt: one QtQuick surface per
physical output, each holding the standard `ext-session-lock` role, and one
controller shared by all of them. The role, private launch, PAM worker and unlock
authority are described in [Native session locking](native-session-lock.md) and
[ADR-0299](../adr/0299-native-locker-private-launch-and-authentication.md). This
page owns what the person at the keyboard sees and when a PAM conversation
exists. The decision record is
[ADR-0382](../adr/0382-start-native-lock-authentication-on-user-input.md).

## Ready state

The screen locks into a ready state. The credential field is visible, enabled and
focused, and no PAM conversation exists. That stays true for the whole time the
screen is locked; the field is never hidden or disabled, so Enter always has
somewhere to go.

A conversation starts only because a person acted:

| Action | Starts an attempt |
| --- | --- |
| First key press (any key but Escape, modifiers included) | Yes; the character stays in the field |
| Mouse click or touch anywhere on the lock surface | Yes |
| Enter, Unlock button or touch-keyboard Enter | Yes, and the typed text answers the first prompt |
| Pointer motion, key release, Escape | No |
| Idle time of any length | No |

`InputActivityFilter` observes the windows and only reports intent; it never
consumes an event. Input seen before the compositor confirmed the lock is
remembered and starts the first attempt when the lock arrives.

## States

`AuthenticationController` moves through four states. Only the first is
persistent.

| State | Meaning | Leaves when |
| --- | --- | --- |
| Idle | Locked, ready, no worker | Input or submit starts an attempt |
| Starting | Worker spawned, no prompt yet | A secret or visible prompt arrives |
| Prompting | A PAM prompt waits for a response | A response is sent |
| Verifying | Response sent, PAM has not answered | PAM asks again, or the attempt ends |

Every ending other than an authenticated unlock returns to Idle by itself and
tells the person why:

| Outcome | Message | Next input |
| --- | --- | --- |
| Denied after a prompt | Authentication failed | Starts a fresh attempt |
| Denied before any prompt | Refused before asking; the account may be temporarily locked | Only Enter retries |
| Account denied | This account cannot unlock this session | Starts a fresh attempt |
| Timed out (90 s) | Timed out; the field is cleared | Starts a fresh attempt |
| Unavailable | Authentication unavailable | Starts a fresh attempt, or Enter if no prompt was ever shown |
| Cancelled (Escape, Cancel) | none; the field is cleared | Starts a fresh attempt |

Nothing restarts an attempt without input. An attempt that died before any prompt
is retried only by an explicit submit, so mashing keys cannot spawn workers.

## Enter, Escape and typed-ahead text

- Characters typed before the first secret prompt are not lost: the first key
  that started the attempt is already in the field and the prompt does not wipe it.
- Enter before the prompt arrives clears the field into the controller, which
  sends the text as soon as the first secret prompt arrives. If the attempt ends
  before any prompt, that text is wiped, never replayed.
- Enter while a response is being verified is ignored and leaves the field alone,
  so a retry typed during `pam_faildelay` is still there afterwards.
- An empty password is never sent: it cannot authenticate and would only count
  as a failed login. Enter on an empty field at a password prompt shows a hint,
  and from the ready state it just starts the attempt. An empty answer to a
  visible prompt is allowed.
- A visible or later prompt discards stale typed text instead of answering it.
- Escape or Cancel kills the attempt, clears the field and returns to Idle.

## Timeouts and `pam_faillock`

The installed `system-auth` counts a conversation that fails inside
`pam_authenticate` as a failed login. The locker is built so only a password a
person submitted can do that:

- The greeter-side `WorkerProcess` times a whole attempt out at 90 s with a
  precise timer and reports `Outcome::TimedOut`; the worker's own channel
  deadline is only a backstop at 105 s, a relation checked at compile time.
- Every abort (cancel, deadline, transport failure) `SIGKILL`s the child before
  closing the channel, so the worker never sees EOF while it can still run.
  `lock.worker-process` proves that a cancelled and a timed-out attempt never
  reach the PAM failure path, and `lock.worker-channel` proves that closing the
  channel without killing does.

## Conditions the greeter survives

| Condition | Behavior |
| --- | --- |
| Zero or oversize configure | Skipped: not applied, not acknowledged. The next usable configure is handled normally (`lock.protocol-integration`) |
| No physical output yet | Waits for `screenAdded` |
| Role refused or QML failed for one output | That output is retried at 250 ms doubling to 30 s with no limit; other outputs keep their windows |
| Output removed during creation or while waiting | Its retry is cancelled and its window destroyed |
| Role handed to a window that dies unshown | The role is released so a retry can succeed |
| Compositor finishes the lock | The locker exits with a warning (also when no window ever got a role, through the role-free observer) |

The compositor keeps the whole session protected black while an output has no
lock surface. Relaunching a locker that did exit is the compositor's job.

## Why it exits and what it says

Each way the process ends logs its reason on stderr (the compositor's journal),
under the categories `qindaqt.lock.greeter` and `qindaqt.lock.protocol`:

| Exit | Log line |
| --- | --- |
| Authenticated unlock processed | `unlock processed by the compositor; the locker is finished` |
| Compositor finished the lock | `the compositor finished the session lock; the locker exits without unlocking` |
| Security precondition not met (exit 2) | `refusing to start (exit 2): <reason>`, one reason per check |
| Event loop ended | `event loop ended normally (exit N)` |

Messages carry state names and counts only, never prompts, responses or other
credential material.

## Composition

| Piece | Responsibility |
| --- | --- |
| `AuthenticationController` | The lifecycle above; borrows two ports; QML sees only `noteInput`, `submit`, `respond`, `begin`, `cancel` and read-only state |
| `LockSessionPort`, `AttemptRunnerPort` | The controller's two collaborators, declared by their consumer; adapters forward to `ProtocolClient` and `WorkerProcess` |
| `InputActivityFilter` | Turns key presses, clicks and touches into "a person is here" |
| `OutputWindowSupervisor`, `RetryBackoff` | Pure per-output retry policy with an injected factory and scheduler |
| `NativeWindows` | Qt glue: windows, timers and logging for the supervisor |

## Private fixture contract

`qindaqt-private-lock-greeter` is driven by the fork's nested compositor
scenarios. `NATIVE_GREETER_UI_READY` means locked with a visible, enabled field;
`NATIVE_GREETER_PROMPT` means a PAM prompt is waiting; `NATIVE_GREETER_IDLE`
means no attempt exists. Its `input` command (controller intent) and `key`
command (a real synthetic key press through window, filter and QML) stand in for
a person and must come before a prompt is expected. See the
[testing harness](../development/testing-harness.md).

## Verification

| Test | Covers |
| --- | --- |
| `lock.greeter-controller` | The state machine with fake ports: no attempt without input, input to attempt to timeout to ready again, Enter from every state, type-ahead, empty password, dead-on-arrival gate, cancel races, approval consumed once, finished lock |
| `lock.greeter-ui` | The real `Main.qml`, controller and filter offscreen: field visible, enabled and focused in every state, first key starts one attempt and keeps its character, Enter always submits |
| `lock.greeter-input` | Which events count as input, and that the filter never consumes |
| `lock.greeter-outputs` | Backoff schedule, per-output retry, removal, reentrancy |
| `lock.protocol-configure` | The configure decision table |
| `lock.protocol-integration` | The real Qt Wayland client and integration plugin against a fake compositor: unusable configures, role release, finished lock |
| `lock.worker-process`, `lock.worker-channel` | `TimedOut`, kill-before-close, the failure-path witness |

The UI, protocol-integration and worker-process tests were each run against the
previous behavior (hidden field, `qFatal` on 0x0, an unreleased role, close
before kill) and fail there; the controller and supervisor tests cover new
types. The nested compositor scenarios and the live session were not exercised
by this change.
