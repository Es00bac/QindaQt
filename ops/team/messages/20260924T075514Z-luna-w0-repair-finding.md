# W0 B3 ownership finding

- Worker: `luna-w0-repair`
- Parent candidate: `5ac3325e9e92cb50a1af283205940fbea5af7e78`
- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w0-wifi-agent`

The new private-bus regression found that `PromptRequest.settingName` shared a backing allocation with the same setting-name key in the incoming settings map. `wipeSettingsMap()` intentionally overwrote that shared storage, so the retained prompt's setting name became zero-filled after admission. The prompt policy now makes an independent setting-name allocation before it retains the prompt. The regression also checks intact `aa{sv}` caller data, the VPN-shaped `a{ss}` request after its `NoSecrets` refusal, prompt metadata, and the successful reply payload. The first repeated CTest run exposed this failure; I am rebuilding and rerunning after the fix.
