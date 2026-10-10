# ACCEPT — faa1f4ca81bdc38944de2e333b8f239bb096439f

- Candidate: faa1f4ca81bdc38944de2e333b8f239bb096439f
- Verdict: ACCEPT; the staging-only StartMenu plugin finding is resolved.

Exact diff from 999c52 adds only `qindaqt_start_menu_qmlplugin` beside `qindaqt-shell`, its dependency rationale and the matching README correction. This supplies the single missing generated install artifact; source/archive/Manifest/signatures, local component/RPATH staging and shell-only final replacement remain unchanged. Diff whitespace gate exits 0. Actual Portage log confirms install completion; root reports staging2 unit exit 0 after clearing only the stale persisted environment/.compiled cache. Signed artifact verification is pending independent review.

Root's properly mapped old R28 ordinary mouse selection raises both test windows and changes stacking; the reported user failure is not reproduced by that case. Source confirms Gather and dock call the same controller activation boundary. The same-window dock/overview comparison can discriminate overview dispatch loss from shared action failure. Native touch remains unmeasured and requires the existing touch-capable test-device flag. No renderer cause or installed resolution is inferred.
