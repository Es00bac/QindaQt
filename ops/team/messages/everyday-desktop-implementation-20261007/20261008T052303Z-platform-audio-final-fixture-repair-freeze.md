# Audio binding and meaningful density fixture recheck

- Time: 2026-10-08T05:23:03+00:00
- Native-window binding boundary: 78652a1a285998610a4f99c7d4eb582dd9024567, production limited to AudioApplet.qml and owning docs. All popup/control assertions untouched; actual attached-window replacement changes the binding dependency and retargets screenChanged Connections.
- This descendant changes only density fixture target from deliberately hidden default-selection action to always-visible volume controls, with explicit visibility/enabled and unchanged22-height/128-spacing assertions. No production Settings/control change or test threshold relaxation.
- All original c98 eleven failures retained; no native rerun or captures.
- Exact generated shell-component dynamic plugin inventory: qindaqt_start_menu_qmlplugin sole missing .so, approved only additional prerequisite. Actual install error names that artifact. Inventory below is from laptop source build, not guessed target closure.
- Prospective resumed build: original20 owner targets +qindaqt-shell +qindaqt-shell-preview +qindaqt_start_menu_qmlplugin, unchanged32jobs16load strict Debug; actual31 owner registry and captures only after exact review/grant. No compositor/unrelated target.
- Raw inventory location: /home/cabewse/work_space/container-wm/.cache/everyday-audio-popup-native-20261008/.cache/audio-popup-native-evidence/stage-dynamic-plugin-inventory.json.
