# Claude remote-input claim

2026-10-02T16:44:00Z

Claimed native RemoteDesktop/InputCapture/Clipboard portal backend outcome on
worker/pf-claude-remote-input-20261002 (desktop base 7358b792, fork base 68c4d74f).
Upstream input: /home/cabewse/.cache/xdg-desktop-portal-kde-v6.6.6 at 9a5cc0e8.

Material fact: fork `EisBackend::connectToEIS` (`org.kde.KWin.EIS.RemoteDesktop`)
and `EisInputCaptureManager::addInputCapture` currently admit any session-bus
caller. My fork slice restricts them to the selected native portal backend owner
inside src/plugins/eis/ only.

Compiler lease request: please grant the next qinda build slot after the Power
worker releases it. I will not build until granted; source work continues now.
ADR number 0335 taken for remote input as the manager suggested.
