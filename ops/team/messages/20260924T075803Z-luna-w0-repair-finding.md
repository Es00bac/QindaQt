# W0 B3 reply-metadata ownership finding

- Worker: `luna-w0-repair`
- Parent candidate: `5ac3325e9e92cb50a1af283205940fbea5af7e78`
- Worktree: `/home/cabewse/work_SPaC3/container-wm/.cache/claude-plan-20260923/w0-wifi-agent`

After separating the prompt's setting name from the incoming settings key, the regression exposed a second alias: `SecretReply.settingName` and each `SecretValue.key` shared storage with the still-live `PromptRequest` metadata. The post-serialization reply-map wipe zeroed the retained prompt setting name and field key. Both values now receive independent allocations when constructing the reply. The private-bus test checks prompt metadata after successful serialization and verifies the exact reply payload. I am rebuilding and rerunning after this correction.
