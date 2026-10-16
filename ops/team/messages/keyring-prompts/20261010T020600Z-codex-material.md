# Material finding and bounded repair

The laptop has one persistent native keyring, with confirmed user preferences `keyring.lockAfterIdleMinutes=10` and `keyring.lockOnScreenLock=true`. Standard unlock requests for a locked collection each launch a helper; three concurrent clients on the installed daemon cause two dismissed requests in the synthetic reproduction (exit 1, 4.373 seconds).

Minimal repair serializes authentication helpers in the existing service, rechecks shared unlocked collection state before a helper and after pacing, preserves caller-owned completions, and restores the initial field on window activation. A queued portal request has no unlock to undo on cancellation. Passwords and real records are never read.

551-page link/navigation validation and strict MkDocs pass. Focused targets and three-client regression remain next. An optional session-lifetime preference change was rejected by automatic approval review for lack of explicit authorization to weaken locked-screen protection; the user was informed and approval requested. Existing preferences are unchanged.
