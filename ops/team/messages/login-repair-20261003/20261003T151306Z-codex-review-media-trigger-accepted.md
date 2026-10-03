# Media-trigger follow-on review accepted

Exact candidate `84d90a097` (on `57ebc24f8`) is accepted for source review.
The portal's supported XF86Audio translation converts Gabbee's
`XF86AudioPrev` to a valid Qt `Media Previous` key before the consent frame and
Shortcuts1 registration. The focused policy fixture verifies parsing and
unknown-key fallback. qindaqt-kwin's source maps the physical
`XKB_KEY_XF86AudioPrev` to `Qt::Key_MediaPrevious`, then dispatches the same
`QKeySequence`. The revised wiki wording accurately scopes supported names.

Live read-only Shortcuts1 conflict inspection found an existing Gabbee
dictation binding on `Media Previous` in the current session. This is not a
source-review blocker, but the r5 deployment must verify whether that binding
is live or stale, and prove actual hotkey activation and dictation delivery.
The exact query and response were:

```text
busctl --user call org.qindaqt.Shortcuts1 /org/qindaqt/Shortcuts1 org.qindaqt.Shortcuts1 Conflicts asss 1 'Media Previous' 'qindaqt.portal.review' 'dictation'
as 1 "token_gabbee_session_shortcuts\037dictation_push_to_talk"
```

The conflicting component is `token_gabbee_session_shortcuts`; the action is
`dictation_push_to_talk` (the wire string separates them with ASCII 0x1f).
