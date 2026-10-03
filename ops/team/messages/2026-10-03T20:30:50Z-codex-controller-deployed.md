# Controller desktop input deployed

- Candidate: `05bd0a45776c34263c4e4a0d0c3f1a02aad4e205` (product source pinned by desktop r11).
- Outcome: Input → Controllers provides Xbox/PlayStation/Nintendo/other family defaults and per-device action/shortcut bindings, hold-to-dictate, stick/touchpad/gyro mouse controls, and Steam/external consumer priority. DualSense USB microphone uses the existing Audio input selection.
- Changed paths: `src/controllers/`, Settings Input model/port/QML/composition, Settings route search/navigation, additive source/test build registries, controller wiki page and ADR 0347.
- Evidence: focused policy and Settings build plus `ctest --test-dir build/controller-focused --output-on-failure` passed 2/2 before the user stopped further testing. Installed-SDK controller plugin compilation exited 0. qinda Portage r11 and qinda-top binary-only r11/KWin r5 installs exited 0. Live plugin load returned `b true`; Controllers1 is owned by existing KWin PID 153380. Real Settings window is running; graphical session remains active.
- User instruction: no additional tests, audits, or whole-desktop rebuilds. No further tests ran after that instruction.
- Bounded caveats: no physical controller or game was exercised. The initial Settings snapshot produces two transient missing-capability bool warnings; the page stays running, and this minor detail was deferred per the user's request.
- Next action: manager may integrate the exact preserved candidate on its integration branch; the user's physical desktop feature is already installed and enabled. No new work is claimed.
