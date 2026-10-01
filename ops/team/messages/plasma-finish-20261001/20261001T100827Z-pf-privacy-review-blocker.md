# Independent repaired native notification privacy review

- Reviewer: pf-privacy-review-sol-20261001
- Time: 2026-10-01T10:08:27+00:00
- Immutable candidate: 1e2ab3ef600a800a49e94280107dd560ddcfd879
- Base: ab7640944d67b49f5fcbd198a96ac6c70cad193c
- Verdict: REJECT — one reproduced denial-transition mapping notification regression.
- Requested next action: same implementer repairs the pre-clear popup count notification and visibility reconciliation; this reviewer rechecks the exact repaired descendant.
- Scope: independent full candidate source diff and bounded original production-consumer reproduction; reviewer did not modify implementation, main, task/feature/queue state.

## Original blocker repaired; new bounded regression

The original immediate disclosure blocker is repaired. Actual Session1 unique-owner replacement, without an event-loop turn and with retained role reads first, denies active/popup/history roles, C++ entries, live policy, popup count, dismiss and invoke. Getter reads produce zero model resets. Production shell now supplies the observer callback rather than relying on the old signal-only policy constructor. The existing default constructors remain compatible scoped APIs and were not treated as production wiring.

A separate regression is reproduced in the ordinary queued denial path. `src/services/notification_presentation_model/src/notification_presentation_privacy.cpp:24` obtains `hadPopups` through `m_popups.rowCount()`. The repaired model intentionally returns0 immediately when policy denies, so `hadPopups` becomes false even when the cached popup stack being cleared contains a notification. Consequently the normal denial never emits `popupCountChanged` from line52–54. The external fixture begins with one popup, the center closed, no operation and no error, then replaces the actual selected Session1 owner:

```text
immediate active_role=0 popup_role=0 history_role=0 entries=0 policy=0 observer=0 popup_count=0 dismiss=0 invoke=0 operations=0 resets=0
after_watchers popup_count=0 popup_count_changed=0 privacy_changed=1 popup_reset=1 center=0 busy=0 error_empty=1
```

`src/shell/runtime/notificationwindowcontroller.cpp:149`–169 subscribes only to popup-count, center, busy and error signals. It does not subscribe to controller privacy or model reset. With that popup-only state, it receives no notification to run `updateVisibility()` after denial. Therefore the previously mapped popup window remains mapped until another unrelated trigger, despite the documented mapped-window hide contract. The model clears private content; this is a surface lifecycle regression, not a claim that already rendered/copied content or physical frames can be recalled. The controller signal defect is directly executed; the mapped-window consequence follows the inspected production connection/visibility code rather than a physical compositor fixture.

Use owned cached popup state (`m_popupEntries`) to preserve the pre-clear notification truth. Connecting the window controller directly to `privatePresentationAllowedChanged` is also appropriate: it owns mapping and should reconcile visibility on privacy transitions independently of whether a model's disclosure count has already closed. Preserve its explicit disconnect/lifetime behavior. Add executable coverage for popup-only denial count notification and exact wiring/visibility transition as practical.

## Exact source and verification evidence

- Reviewed observer/public-attachment/native monitor/transport trust paths: supervisor-provisioned PID, daemon owner/PID/UID, pinned selected Session1 owner, kernel ordinary socket peer/pidfd, actual same bus, targeted native receipt acknowledgement/nonce/signature. No fallback, new locker authority, host storage dependency or platform mutation.
- Observer owns monitor/transport/attachment in safe declaration and stop order. Shell teardown resets presentation before policy before observer. Model callbacks call the thread-confined policy without emitting or resetting; policy exceptions deny. Normal watcher invalidation clears projections and next unlock baselines authoritative state without replay.
- qinda candidate source directly checked HEAD1e2ab3ef, clean; existing immutable build uses that source root, Debug/shared ON/plugin OFF. Final preserved repair build log includes completed production-shell and focused-target links. Independent tests reused those exact-candidate immutable binaries; this reviewer rebuilt only an external2-action probe using their public libraries and observer object.

Commands/results:

```sh
QT_FATAL_WARNINGS=1 QT_QPA_PLATFORM=offscreen ctest --test-dir /home/cabewse/work_SPaC3/container-wm.worktrees/pf-shell-lock-20261001/build/focused --output-on-failure --no-tests=error -R '^qindaqt[.](native-lock-monitor|native-lock-qt-transport|notification-privacy-policy|notification-presentation-model|notification-presentation-dnd|notification-presentation-privacy|notification-presentation-app-policy|shell-native-notification-lock|compositor-attachment)$'
python3 tools/validate-docs
mkdocs build --strict --site-dir .cache/reviewer-docs
git diff --check ab764094 1e2ab3ef
python3 tools/check-source-shape --largest 0 --json
python3 tools/check-source-shape --root /home/cabewse/work_space/container-wm-removable-media --largest 0 --json
ninja -C /home/cabewse/.cache/pf-privacy-review-sol-20261001 -j24 -l24
QT_FATAL_WARNINGS=1 /home/cabewse/.cache/pf-privacy-review-sol-20261001/probe
```

- Focused CTests PASS9/9 exit0, zero skips,6.87s. Test log qinda:/home/cabewse/.cache/pf-privacy-review-sol-20261001-tests.log.
- Link/navigation476 documents PASS exit0; strict MkDocs PASS exit0; diff check PASS exit0. The first MkDocs invocation failed before starting because ignored .cache was absent; creating that directory fixed the invocation, with no dependency/install changes. Repository link checker is `tools/validate-docs`; no `tools/check-doc-links` exists here.
- Source-shape exit1 on both candidate and exact baseab764094:57 existing errors, no new issue keys;5329 versus5326 files. Existing shell runtime/function and test-CMake size errors remain nonblocking inherited debt. Newly introduced observer/policy/model methods are bounded.
- External probe compile/link PASS2/2 exit0; private probe exit0 means this blocker reproduced. Source/rules/preparation/logs retained at qinda:/home/cabewse/.cache/pf-privacy-review-sol-20261001/{probe.cpp,build.ninja,prepare_probe.py,build.log,probe.log}. It includes the exact candidate's native fixture and controller fixture, uses the exact new production observer-policy callback and controller-created models, and performs actual private-bus owner replacement. No product source/build regeneration was performed.

## Stopping point and concrete help

Compiler/private-runtime slots released to root; no live host bus, lock, suspend, service, password, install or physical-session action. Read Shell/Platform queue and relevant blocker/claim thread. Compatible help offered: same reviewer can recompile this bounded probe and recheck nine focused rows plus wiring/visibility coverage on the exact repaired descendant. Current status is waiting for that descendant; no completion/integration claim.

## Durable external probe source

```cpp
#define main native_fixture_main
#include "/home/cabewse/work_SPaC3/container-wm.worktrees/pf-shell-lock-20261001/tests/shell/tst_native_notification_lock_observer.cpp"
#undef main
#define main controller_fixture_main
#include "/home/cabewse/work_SPaC3/container-wm.worktrees/pf-shell-lock-20261001/tests/services/notification_presentation_model/tst_notification_presentation_privacy.cpp"
#undef main
#include <cstdio>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    PrivateSessionBus fixture;
    if (!fixture.start()) return 10;
    auto server = fixture.connect("review-server-"), session = fixture.connect("review-session-"),
         replacement = fixture.connect("review-replacement-"), connection = fixture.connect("review-client-");
    const auto name = QStringLiteral("org.qindaqt.Session1");
    if (!session.registerService(name)) return 11;
    NativeBackend backend(server);
    if (!backend.expose()) return 12;
    QTemporaryDir runtime;
    const auto basename = QString(QindaQt::CompositorNames::waylandSocketPrefix) + "0";
    OrdinaryListener listener(runtime.filePath(basename));
    if (!listener.ready) return 13;
    NativeNotificationLockObserver observer(connection, getpid(), runtime.path(), basename);
    NotificationPresentationPolicy::NotificationPrivacyPolicy privacy([&] { return observer.contentMayBeShown(); });
    QObject::connect(&observer, &NativeNotificationLockObserver::contentMayBeShownChanged,
        &privacy, &NotificationPresentationPolicy::NotificationPrivacyPolicy::setPrivatePresentationAllowed);
    privacy.setPrivatePresentationAllowed(observer.contentMayBeShown());
    if (!observer.start() || !QTest::qWaitFor([&] { return privacy.privatePresentationAllowed(); }, 2000)) return 14;
    FakeTransport transport;
    NotificationPresentationClient::NotificationPresentationClient client(transport, token(), clientTiming());
    NotificationPresentationPolicy::NotificationInterruptionPolicy interruption;
    auto timing = presentationTiming();
    timing.normalUrgencyMilliseconds = timing.criticalUrgencyMilliseconds = 10000;
    NotificationPresentationModel::NotificationPresentationController controller(client, interruption, privacy, timing);
    if (!client.start()) return 15;
    const auto owner = QStringLiteral(":1.101"), epoch = QStringLiteral("90909090-9090-9090-9090-909090909090");
    transport.owner(owner);
    if (!QTest::qWaitFor([&] { return transport.requests.size() == 1; }, 2000)) return 16;
    transport.reply(transport.requests.last(), wire(epoch, 1,
        {notification(39, QStringLiteral("Removed private")), notification(40, QStringLiteral("Baseline"))}));
    transport.changed(owner, epoch, 2);
    if (!QTest::qWaitFor([&] { return transport.requests.size() == 2; }, 2000)) return 17;
    transport.reply(transport.requests.last(), wire(epoch, 2,
        {notification(40, QStringLiteral("Baseline")), notification(41, QStringLiteral("Fresh private"), 2)}));
    auto *active = controller.activeModel(); auto *popups = controller.popupModel(); auto *history = controller.historyModel();
    const auto ai = active->index(0, 0), pi = popups->index(0, 0), hi = history->index(0, 0);
    if (controller.popupCount() != 1 || !ai.isValid() || !pi.isValid() || !hi.isValid()) return 18;
    QSignalSpy popupChanged(&controller, &NotificationPresentationModel::NotificationPresentationController::popupCountChanged);
    QSignalSpy privacyChanged(&controller, &NotificationPresentationModel::NotificationPresentationController::privatePresentationAllowedChanged);
    QSignalSpy activeReset(active, &QAbstractItemModel::modelReset), popupReset(popups, &QAbstractItemModel::modelReset), historyReset(history, &QAbstractItemModel::modelReset);
    if (!session.unregisterService(name) || !replacement.registerService(name)) return 19;
    // First reads after actual replacement are retained role reads, before an observer getter or event-loop turn.
    const bool activeRole = active->data(ai, NotificationPresentationModel::NotificationListModel::SummaryRole).isValid();
    const bool popupRole = popups->data(pi, NotificationPresentationModel::NotificationListModel::BodyRole).isValid();
    const bool historyRole = history->data(hi, NotificationPresentationModel::NotificationListModel::SummaryRole).isValid();
    const bool modelEntries = !static_cast<NotificationPresentationModel::NotificationListModel *>(active)->entries().isEmpty();
    const bool allowed = privacy.privatePresentationAllowed(), observed = observer.contentMayBeShown();
    const bool dismissed = controller.dismiss(40), invoked = controller.invokeAction(40, QStringLiteral("open"));
    std::printf("immediate active_role=%d popup_role=%d history_role=%d entries=%d policy=%d observer=%d popup_count=%d dismiss=%d invoke=%d operations=%lld resets=%lld\n",
        activeRole, popupRole, historyRole, modelEntries, allowed, observed, controller.popupCount(), dismissed, invoked,
        static_cast<long long>(transport.operations.size()), static_cast<long long>(activeReset.size()+popupReset.size()+historyReset.size()));
    if (activeRole || popupRole || historyRole || modelEntries || allowed || observed || dismissed || invoked || !transport.operations.isEmpty() || !activeReset.isEmpty() || !popupReset.isEmpty() || !historyReset.isEmpty()) return 20;
    if (!QTest::qWaitFor([&] { return !privacyChanged.isEmpty(); }, 2000)) return 21;
    std::printf("after_watchers popup_count=%d popup_count_changed=%lld privacy_changed=%lld popup_reset=%lld center=%d busy=%d error_empty=%d\n",
        controller.popupCount(), static_cast<long long>(popupChanged.size()), static_cast<long long>(privacyChanged.size()), static_cast<long long>(popupReset.size()), controller.centerOpen(), controller.operationBusy(), controller.operationErrorText().isEmpty());
    return popupChanged.isEmpty() ? 0 : 1; // zero means mapping-notification blocker reproduced
}
```
