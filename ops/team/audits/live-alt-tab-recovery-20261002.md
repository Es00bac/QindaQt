# Read-only live Alt-Tab recovery investigation

Requested bound: five minutes, source/read-only only while manager recovers the physical laptop desktop. No compositor restart/logout, host input, debugger/injection, build or runtime was performed.

Fresh laptop evidence: process814278 remains `kwin_wayland --drm --socket qindaqt-0 ... --exit-with-session qindaqt-session`, started September30. Supervisor814357 remains. New session bus service listing contains neither org.kde.KWin nor org.kde.kglobalaccel. Recovered shell was3285705 in the fresh listing (prior shell814371 absent), so its restart has not restored compositor bus ownership.

Upstream exact KGlobalAccelD6.6.6 source59fbb109bd449aca3167df65cf3526fe31a81bcb registers service/object during init. No reconnect entry appears in that class. Component.invokeShortcut emits the component press event; it cannot reach an absent owner on the current bus. Sources: [daemon init](https://raw.githubusercontent.com/KDE/kglobalacceld/59fbb109bd449aca3167df65cf3526fe31a81bcb/src/kglobalacceld.cpp), [component dispatch](https://raw.githubusercontent.com/KDE/kglobalacceld/59fbb109bd449aca3167df65cf3526fe31a81bcb/src/component.cpp).

Project src/shell/runtime/edgegesturesubscriber.cpp invokeWalkThroughWindows calls org.kde.kglobalaccel /component/kwin invokeShortcut("Walk Through Windows"); the header describes it as the public task-switcher door. Restarting shell/desktop-controls cannot reconnect the in-process KWin shortcut service. Installed qindaqt-desktop-controls source is a media-key/idle helper, not a window-control CLI. No installed native window-control CLI was found in bounded executable/source inventory.

Conclusion: no verified safe in-process Alt-Tab recovery hook found. Do not launch a competing standalone kglobalacceld as a speculative repair. A temporary mouse-selected dock/Workspace Switcher path may remain usable through existing Wayland controls; that is an inference, not a tested shortcut repair. Restoring actual Alt-Tab appears to require a manager-controlled session relogin after saving work if the compositor cannot regain its bus connection. This conclusion is bounded source/process evidence, not proof that every possible KWin recovery method is absent.

Native runtime qualification stays on hold. Exact import repair97024c8092031f0ec747472e6b0c7c2baa0bb530 is now independently source accepted by manager. Shared runtime source remains5f164 unchanged until recovery release.
