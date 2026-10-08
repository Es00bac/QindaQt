# First actual Bluetooth strict compile failure and narrow repair

Exact8c02 configured successfully (39.1s configure,15.5s generate). The original combined build argv failed before compilation on an applet target excluded by Shell=OFF. Root approved main registered targets plus the existing standalone applet harness sequentially; original argv/log remain.

Corrected main build used strict -Wall/-Wextra/-Wpedantic/-Wconversion/-Wsign-conversion/-Wshadow/-Werror and -j24 -l24. It stopped after1743/2150 actions because qt_radio_power_port.cpp146 declared reply-local now shadowing dispatch-local now123. Actual compiler diagnostic and full argv are preserved in .cache/bluetooth-native-logs/build-8c02-corrected.log. No CTest, fixture, host or radio action occurred.

The only product repair renames the reply-local variable to replyTime and updates its two identical deadline predicates. No behavior, warning policy or test assertion changes. Diff check passes. Same reviewer recheck requested before further build. The compiler process has settled; the focused manager lease remains reserved pending prompt repair recheck.
